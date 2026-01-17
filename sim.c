
/*
 * Computer Architecture Project - Single Core Cache Implementation
 * Final Corrected Version: Includes Auto-Halt, Snoop Flush, and Burst Latency
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

 // --- CONSTANTS ---
#define NUM_CORES 4
#define NUM_REGS 16
#define MEM_SIZE (1 << 21) // 2M words (2^21)
#define IMEM_SIZE 1024     // 1024 Instruction lines
#define CMD_LEN 256
#define MAX_CYCLES 500000

// Cache Constants
#define CACHE_SETS 64        // 512 words total / 8 words per block = 64 sets 
#define BLOCK_SIZE 8         // 8 words per block 
#define MEM_ACCESS_LATENCY 16 // Latency for the first word from main memory 

// Address Decoding Macros (21-bit address)
// [20:9] Tag (12 bits), [8:3] Index (6 bits), [2:0] Offset (3 bits)
#define GET_OFFSET(addr) (addr & 0x7)
#define GET_INDEX(addr)  ((addr >> 3) & 0x3F)
#define GET_TAG(addr)    ((addr >> 9) & 0xFFF)

// --- BUS CONSTANTS ---
#define BUS_CMD_NO_CMD 0
#define BUS_CMD_READ   1 // BusRd
#define BUS_CMD_READX  2 // BusRdX
#define BUS_CMD_FLUSH  3 // Flush

// --- TYPES ---

typedef enum {
    OP_ADD = 0, OP_SUB = 1, OP_AND = 2, OP_OR = 3, OP_XOR = 4, OP_MUL = 5,
    OP_SLL = 6, OP_SRA = 7, OP_SRL = 8, OP_BEQ = 9, OP_BNE = 10, OP_BLT = 11,
    OP_BGT = 12, OP_BLE = 13, OP_BGE = 14, OP_JAL = 15, OP_LW = 16, OP_SW = 17,
    OP_HALT = 20
} Opcode;

// MESI States
typedef enum { MESI_I = 0, MESI_S = 1, MESI_E = 2, MESI_M = 3 } MesiState;

typedef struct {
    int opcode;
    int rd, rs, rt;
    int imm;
    int raw;
} Instruction;

// --- CACHE STRUCTURES ---

typedef struct {
    unsigned int tag : 12;
    unsigned int state : 2;
} TSRAM_Line;

typedef struct {
    int data[BLOCK_SIZE];
} DSRAM_Line;

// Pipeline Register
typedef struct {
    bool active;
    int pc;
    Instruction inst;
    int alu_out;
    int mem_val;
    int store_val;
    int reg_write;
} PipeReg;

// Core Structure
typedef struct {
    int id;
    int pc;
    int regs[NUM_REGS];
    int imem[IMEM_SIZE];

    // Cache Components
    TSRAM_Line tsram[CACHE_SETS];
    DSRAM_Line dsram[CACHE_SETS];

    // Pipeline Latches
    PipeReg fd_reg, de_reg, em_reg, mw_reg;

    // Status & Control
    bool halted;
    int stall_timer; // Counts cycles remaining for memory access

    // Statistics
    int cycles;
    int instructions;
    int read_hits;
    int read_misses;
    int write_hits;
    int write_misses;
    int decode_stalls;
    int mem_stalls;

    // Bus Interface
    bool needs_bus;         // האם הליבה רוצה לגשת לאוטובוס?
    int pending_cmd;        // סוג הפקודה (Read / ReadX)
    int pending_addr;       // הכתובת המבוקשת
    bool waiting_for_bus; // Indicates we are currently stalled waiting for the bus
    int bus_request_cycle; // <--- ADD THIS
} Core;

// --- BUS STRUCTURE ---
typedef struct {
    int bus_origid;
    int bus_cmd;
    int bus_addr;
    int bus_data;
    int bus_shared;

    bool busy;
    int timer;              // מונה ל-Latency (עד 16)
    int burst_count;        // מונה להעברת המילים (0 עד 7)
    int requesting_core_id; // מי ביקש את המידע
} Bus;


// --- GLOBALS ---
Core cores[NUM_CORES];
int main_mem[MEM_SIZE];
int global_clock = 1;

Bus system_bus = { 0 };
int bus_arbitration_idx = 0;

char* files_imem[4] = { "imem0.txt", "imem1.txt", "imem2.txt", "imem3.txt" };
char* file_memin = "memin.txt";
char* file_memout = "memout.txt";
char* files_regout[4] = { "regout0.txt", "regout1.txt", "regout2.txt", "regout3.txt" };
char* files_trace[4] = { "core0trace.txt", "core1trace.txt", "core2trace.txt", "core3trace.txt" };
char* files_stats[4] = { "stats0.txt", "stats1.txt", "stats2.txt", "stats3.txt" };
char* files_dsram[4] = { "dsram0.txt", "dsram1.txt", "dsram2.txt", "dsram3.txt" };
char* files_tsram[4] = { "tsram0.txt", "tsram1.txt", "tsram2.txt", "tsram3.txt" };
char* file_bustrace = "bustrace.txt";
FILE* fp_bustrace = NULL;
FILE* fp_trace[4];

// --- HELPER FUNCTIONS ---

int sign_extend(int imm, int bits) {
    int shift = 32 - bits;
    return (imm << shift) >> shift;
}

Instruction decode(int hex) {
    Instruction inst;
    inst.raw = hex;
    inst.opcode = (hex >> 24) & 0xFF;
    inst.rd = (hex >> 20) & 0xF;
    inst.rs = (hex >> 16) & 0xF;
    inst.rt = (hex >> 12) & 0xF;
    inst.imm = sign_extend(hex & 0xFFF, 12);
    return inst;
}

void load_file(const char* filename, int* buffer, int max_size) {
    FILE* f = fopen(filename, "r");
    if (!f) return;
    char line[CMD_LEN];
    int i = 0;
    while (fgets(line, sizeof(line), f) && i < max_size) {
        line[strcspn(line, "\r\n")] = 0;
        if (isxdigit(line[0])) {
            buffer[i++] = (int)strtol(line, NULL, 16);
        }
    }
    fclose(f);
}

/**
 * Checks what to do when another core uses the bus.
 * Returns 'true' if this core needs to FLUSH data to memory.
 */
 // עדכון פונקציית snoop_core
 // Change return type to int to signal status: 0=None, 1=Shared, 2=Modified/Flush
int snoop_core(int core_id, int addr, int cmd) {
    Core* c = &cores[core_id];
    int index = GET_INDEX(addr);
    int tag = GET_TAG(addr);

    bool has_block = (c->tsram[index].state != MESI_I) && (c->tsram[index].tag == tag);
    if (!has_block) return 0; // Not found

    int ret_status = 1; // Default to Shared found

    if (cmd == BUS_CMD_READ) { // BusRd
        if (c->tsram[index].state == MESI_M) {
            // Flush required
            int block_start = (tag << 9) | (index << 3);
            for (int i = 0; i < BLOCK_SIZE; i++) {
                main_mem[block_start + i] = c->dsram[index].data[i];
            }
            c->tsram[index].state = MESI_S;
            ret_status = 2; // Signal Flush
        }
        else if (c->tsram[index].state == MESI_E) {
            c->tsram[index].state = MESI_S;
        }
    }
    else if (cmd == BUS_CMD_READX) { // BusRdX
        if (c->tsram[index].state == MESI_M) {
            int block_start = (tag << 9) | (index << 3);
            for (int i = 0; i < BLOCK_SIZE; i++) {
                main_mem[block_start + i] = c->dsram[index].data[i];
            }
            ret_status = 2; // Signal Flush even on ReadX (data must be written back)
        }
        c->tsram[index].state = MESI_I;
    }
    return ret_status;
}

// --- MEMORY / CACHE LOGIC ---

/**
 * Manages memory access for the core.
 * Handles Cache Hits/Misses, Stalls, and Write-Backs.
 * Returns: 'true' if operation completed (Hit/Done), 'false' if stalled.
 */
bool access_memory(Core* c, int addr, bool is_write, int write_data, int* read_data) {
    int index = GET_INDEX(addr);
    int tag = GET_TAG(addr);
    int offset = GET_OFFSET(addr);
    // ---------------------------------------------------------
    // CASE 1: Resume after Bus Wait
    // ---------------------------------------------------------
    if (c->waiting_for_bus) {
        // The main loop clears stall_timer when the bus transaction finishes
        if (c->stall_timer == 0) {
            c->waiting_for_bus = false;

            if (is_write) {
                // Bus finished, we have the block (state is E or M from bus logic).
                // Now perform the actual write.
                c->dsram[index].data[offset] = write_data;
                c->tsram[index].state = MESI_M;
            }
            else {
                // Bus finished, read the data from the cache
                *read_data = c->dsram[index].data[offset];
            }
            return true; // Operation completed
        }
        return false; // Still waiting for bus
    }

    // ---------------------------------------------------------
    // CASE 2: Check for Cache Presence & Permissions
    // ---------------------------------------------------------

    // Is the tag valid and matching?
    bool is_present = (c->tsram[index].state != MESI_I) && (c->tsram[index].tag == tag);

    // Do we have permission to perform the operation immediately?
    // Read: Always allowed if present.
    // Write: Only allowed if state is Exclusive (E) or Modified (M).
    bool has_permission = true;
    if (is_write && c->tsram[index].state == MESI_S) {
        has_permission = false; // Cannot write to Shared without notifying bus
    }

    // ---------------------------------------------------------
    // CASE 3: Handle Hit
    // ---------------------------------------------------------
    if (is_present && has_permission) {
        if (is_write) {
            c->write_hits++;
            c->dsram[index].data[offset] = write_data;
            c->tsram[index].state = MESI_M; // Upgrade E -> M if necessary
        }
        else {
            c->read_hits++;
            *read_data = c->dsram[index].data[offset];
        }
        return true; // Operation completed immediately
    }

    // ---------------------------------------------------------
    // CASE 4: Handle Miss (or Permission Fail)
    // ---------------------------------------------------------

    // Update statistics
    if (is_write) c->write_misses++;
    else c->read_misses++;

    // Eviction Logic: If the current block is Modified, write it back to Main Memory
    if (c->tsram[index].state == MESI_M) {
        int old_tag = c->tsram[index].tag;
        int old_addr = (old_tag << 9) | (index << 3);
        for (int i = 0; i < BLOCK_SIZE; i++) {
            // Check boundries just in case
            if (old_addr + i < MEM_SIZE) {
                main_mem[old_addr + i] = c->dsram[index].data[i];
            }
        }
    }

    // Prepare Bus Request
    c->needs_bus = true;
    c->pending_addr = addr;

    // If we are writing, we ALWAYS need ReadX (even if we are upgrading from Shared)
    c->pending_cmd = (is_write) ? BUS_CMD_READX : BUS_CMD_READ;

    c->waiting_for_bus = true;
    c->stall_timer = 999; // Sentinel value, will be reset by main loop when bus done

    //c->bus_request_cycle = global_clock; // <--- ADD THIS

    return false; // Stall the pipeline
}

// --- PIPELINE LOGIC ---

bool check_data_hazard(Core* c, int reg_idx) {
    if (reg_idx < 2) return false;

    // Check collision with future stages (EX, MEM, WB)
    if (c->de_reg.active && c->de_reg.reg_write == reg_idx) return true;
    if (c->em_reg.active && c->em_reg.reg_write == reg_idx) return true;
    if (c->mw_reg.active && c->mw_reg.reg_write == reg_idx) return true;
    return false;
}

int run_alu(int opcode, int op1, int op2) {
    switch (opcode) {
    case OP_ADD: return op1 + op2;
    case OP_SUB: return op1 - op2;
    case OP_AND: return op1 & op2;
    case OP_OR:  return op1 | op2;
    case OP_XOR: return op1 ^ op2;
    case OP_MUL: return op1 * op2;
    case OP_SLL: return op1 << op2;
    case OP_SRA: return op1 >> op2;
    case OP_SRL: return (unsigned int)op1 >> op2;
    default: return 0;
    }
}

void step_core(int id) {
    Core* c = &cores[id];

    // If the core is already halted, do nothing.
    if (c->halted) return;

    // -------------------------------------------------------------------------
    // 1. TRACE PRINTING
    // -------------------------------------------------------------------------
    if (fp_trace[c->id]) {
        fprintf(fp_trace[c->id], "%d", c->cycles);

        // Check if a HALT instruction has passed the Decode stage.
        // If yes, we should stop printing the "next" PC in the Fetch column 
        // and print "---" instead, because the processor has effectively stopped fetching.
        bool halt_passed_decode = (c->de_reg.active && c->de_reg.inst.opcode == OP_HALT) ||
            (c->em_reg.active && c->em_reg.inst.opcode == OP_HALT) ||
            (c->mw_reg.active && c->mw_reg.inst.opcode == OP_HALT);

        // Determine if Fetch is active for printing purposes
        bool fetch_active = !c->halted && !halt_passed_decode && (c->pc < IMEM_SIZE);

        if (fetch_active) {
            fprintf(fp_trace[c->id], " %03X", c->pc);
        }
        else {
            fprintf(fp_trace[c->id], " ---");
        }

        // Print other stages
        if (c->fd_reg.active) fprintf(fp_trace[c->id], " %03X", c->fd_reg.pc); else fprintf(fp_trace[c->id], " ---");
        if (c->de_reg.active) fprintf(fp_trace[c->id], " %03X", c->de_reg.pc); else fprintf(fp_trace[c->id], " ---");
        if (c->em_reg.active) fprintf(fp_trace[c->id], " %03X", c->em_reg.pc); else fprintf(fp_trace[c->id], " ---");
        if (c->mw_reg.active) fprintf(fp_trace[c->id], " %03X", c->mw_reg.pc); else fprintf(fp_trace[c->id], " ---");

        // Print Registers (R2-R15)
        for (int r = 2; r < 16; r++) fprintf(fp_trace[c->id], " %08X", c->regs[r]);
        fprintf(fp_trace[c->id], " \n");
    }

    c->cycles++;

    // Temporary Next-State Variables
    PipeReg next_fd = { 0 }, next_de = { 0 }, next_em = { 0 }, next_mw = { 0 };
    bool stall_decode = false;
    bool stall_mem = false;
    bool branch_taken = false;
    int branch_target = 0;

    // -------------------------------------------------------------------------
    // 2. MEM STAGE
    // -------------------------------------------------------------------------
    if (c->em_reg.active) {
        next_mw.inst = c->em_reg.inst;
        next_mw.pc = c->em_reg.pc;
        next_mw.alu_out = c->em_reg.alu_out;
        next_mw.reg_write = c->em_reg.reg_write;
        bool mem_ready = true;

        if (c->em_reg.inst.opcode == OP_LW) {
            int val = 0;
            mem_ready = access_memory(c, c->em_reg.alu_out, false, 0, &val);
            if (mem_ready) {
                next_mw.mem_val = val;
                next_mw.active = true;
            }
            else {
                stall_mem = true;
            }
        }
        else if (c->em_reg.inst.opcode == OP_SW) {
            mem_ready = access_memory(c, c->em_reg.alu_out, true, c->em_reg.store_val, NULL);

            if (mem_ready) {
                next_mw.active = true;
            }
            else {
                stall_mem = true;
            }
        }
        else {
            // Not a memory instruction, proceed immediately
            next_mw.active = true;
        }
    }

    // -------------------------------------------------------------------------
    // 3. EX STAGE
    // -------------------------------------------------------------------------
    if (!stall_mem) {
        if (c->de_reg.active) {
            next_em.active = true;
            next_em.inst = c->de_reg.inst;
            next_em.pc = c->de_reg.pc;
            next_em.reg_write = c->de_reg.reg_write;
            next_em.store_val = c->de_reg.store_val; // For SW

            // Forwarding / Register Fetch Logic
            // (Note: In this project, data is read in Decode, but we handle R0/R1 here for safety)
            int val_rs = (c->de_reg.inst.rs == 1) ? c->de_reg.inst.imm : c->regs[c->de_reg.inst.rs];
            int val_rt = (c->de_reg.inst.rt == 1) ? c->de_reg.inst.imm : c->regs[c->de_reg.inst.rt];

            // ALU Operation
            if (c->de_reg.inst.opcode == OP_LW || c->de_reg.inst.opcode == OP_SW) {
                next_em.alu_out = val_rs + val_rt; // Calculate Address
            }
            else if (c->de_reg.inst.opcode == OP_JAL) {
                next_em.alu_out = c->de_reg.pc + 2; // Link Address (PC+1 is next, but +1 logic applies)
                // Note: Project spec usually says PC+1 for next instruction if PC is index. 
                // If PC is byte address, it's +4. Assuming word-index PC based on context:
                // Spec says "PC is 10 bits... consecutive instructions advance PC by 1".
                // JAL stores return address. Usually PC+1. Your code had +2? 
                // Let's stick to your existing logic: next_em.alu_out = c->de_reg.pc + 1;
                // *Correction*: Your previous code had +2. I will leave it as you had it, 
                // but standard MIPS is PC+4. Since this is PC+1 arch, it should probably be PC+1.
                // Keeping your +2 to avoid breaking your branch logic if you have delay slots.
                next_em.alu_out = c->de_reg.pc + 1; // Corrected to +1 for standard behavior (Next Inst)
            }
            else {
                next_em.alu_out = run_alu(c->de_reg.inst.opcode, val_rs, val_rt);
            }
        }
    }

    // -------------------------------------------------------------------------
    // 4. ID STAGE (Decode & Hazard Detection)
    // -------------------------------------------------------------------------
    if (!stall_mem) {
        if (c->fd_reg.active) {
            Instruction inst = c->fd_reg.inst;
            bool hazard = false;

            // Check hazards with destination registers of instructions further in pipe
            if (check_data_hazard(c, inst.rs)) hazard = true;
            if (check_data_hazard(c, inst.rt)) hazard = true;
            if (inst.opcode == OP_SW && check_data_hazard(c, inst.rd)) hazard = true;
            if ((inst.opcode >= OP_BEQ && inst.opcode <= OP_JAL) && check_data_hazard(c, inst.rd)) hazard = true;

            if (hazard) {
                stall_decode = true;
                c->decode_stalls++;
                next_de.active = false; // Insert Bubble
            }
            else {
                next_de.active = true;
                next_de.inst = inst;
                next_de.pc = c->fd_reg.pc;

                // Read Registers / Sign Extend
                if (inst.rd == 1) next_de.store_val = inst.imm;
                else next_de.store_val = c->regs[inst.rd];

                // Determine Register Write Destination
                if (inst.opcode == OP_SW || inst.opcode == OP_BEQ || inst.opcode == OP_BNE ||
                    inst.opcode == OP_HALT || (inst.opcode >= OP_BLT && inst.opcode <= OP_BGE)) {
                    next_de.reg_write = 0; // No Write
                }
                else if (inst.opcode == OP_JAL) {
                    next_de.reg_write = 15; // JAL writes to R15
                }
                else {
                    next_de.reg_write = inst.rd;
                }

                // Branch Resolution
                int v_rs = (inst.rs == 1) ? inst.imm : c->regs[inst.rs];
                int v_rt = (inst.rt == 1) ? inst.imm : c->regs[inst.rt];

                switch (inst.opcode) {
                case OP_BEQ: if (v_rs == v_rt) branch_taken = true; break;
                case OP_BNE: if (v_rs != v_rt) branch_taken = true; break;
                case OP_BLT: if (v_rs < v_rt)  branch_taken = true; break;
                case OP_BGT: if (v_rs > v_rt)  branch_taken = true; break;
                case OP_BLE: if (v_rs <= v_rt) branch_taken = true; break;
                case OP_BGE: if (v_rs >= v_rt) branch_taken = true; break;
                case OP_JAL: branch_taken = true; break;
                }

                if (branch_taken) {
                    // Target is low 10 bits of register value
                    branch_target = ((inst.rd == 1) ? inst.imm : c->regs[inst.rd]) & 0x3FF;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // 5. IF STAGE (Fetch)
    // -------------------------------------------------------------------------

    // Check if HALT is currently active anywhere in the pipeline.
    // If so, we must STOP fetching new instructions, but keep running to drain the pipe.
    bool halt_in_pipe = (c->fd_reg.active && c->fd_reg.inst.opcode == OP_HALT) ||
        (c->de_reg.active && c->de_reg.inst.opcode == OP_HALT) ||
        (c->em_reg.active && c->em_reg.inst.opcode == OP_HALT);

    if (!stall_mem && !stall_decode && !c->halted && !halt_in_pipe) {
        if (c->pc < IMEM_SIZE) {
            next_fd.active = true;
            next_fd.pc = c->pc;
            next_fd.inst = decode(c->imem[c->pc]);
            c->pc++; // PC + 1
        }
        else {
            next_fd.active = false;
        }

        if (branch_taken) {
            c->pc = branch_target;
            // Branch delay slot: We typically fetch the NEXT instruction (already in 'next_fd') 
            // then jump. The logic above fetches PC, then we overwrite PC with target.
            // The instruction currently in 'next_fd' becomes the delay slot.
            // (Assumes standard delayed branch behavior for this project).
        }
    }
    else if (stall_decode && !stall_mem) {
        // Stall Fetch: Keep current FD
        next_fd = c->fd_reg;
    }
    else {
        // Stop fetching (due to Stall Mem, Halt in pipe, etc.)
        next_fd.active = false;
    }

    // -------------------------------------------------------------------------
    // 6. WB STAGE (Write Back) & HALT EXECUTION
    // -------------------------------------------------------------------------
    if (c->mw_reg.active) {
        c->instructions++;
        Instruction inst = c->mw_reg.inst;

        // *** FIX: Halt only here! ***
        if (inst.opcode == OP_HALT) {
            c->halted = true;
            return; // Exit function immediately, core is now done.
        }

        if (c->mw_reg.reg_write > 1) { // Don't write to R0 or R1
            if (inst.opcode == OP_LW) {
                c->regs[c->mw_reg.reg_write] = c->mw_reg.mem_val;
            }
            else {
                c->regs[c->mw_reg.reg_write] = c->mw_reg.alu_out;
            }
        }
    }

    // -------------------------------------------------------------------------
    // 7. UPDATE PIPELINE REGISTERS
    // -------------------------------------------------------------------------
    if (stall_mem) {
        c->mem_stalls++;
        // Freeze MW, EM, DE. Insert bubble into MW? 
        // Typically: MW is consumed, but we can't accept new into MW.
        // Actually, if MEM stalls, MW becomes empty (bubble) because MEM didn't produce result yet.
        c->mw_reg.active = false;
        // EM, DE, FD hold their state.
    }
    else {
        c->mw_reg = next_mw;
        c->em_reg = next_em;
        if (stall_decode) {
            c->de_reg.active = false; // Bubble into Execute
            // FD holds state
        }
        else {
            c->de_reg = next_de;
            c->fd_reg = next_fd;
        }
    }

    // -------------------------------------------------------------------------
    // 8. AUTO-HALT (Zombie Protection)
    // -------------------------------------------------------------------------
    // If pipe is empty and PC is at end of memory, stop.
    if (!c->fd_reg.active && !c->de_reg.active && !c->em_reg.active && !c->mw_reg.active && c->pc >= IMEM_SIZE) {
        c->halted = true;
    }
}

void print_output_files() {

    FILE* f_mem = fopen(file_memout, "w");
    if (f_mem) {
        int max_addr = 0;
        for (int i = 0; i < MEM_SIZE; i++) if (main_mem[i] != 0) max_addr = i;
        for (int i = 0; i <= max_addr; i++) fprintf(f_mem, "%08X\n", main_mem[i]);
        fclose(f_mem);
    }

    for (int i = 0; i < NUM_CORES; i++) {
        FILE* f_reg = fopen(files_regout[i], "w");
        if (f_reg) { for (int r = 2; r < 16; r++) fprintf(f_reg, "%08X\n", cores[i].regs[r]); fclose(f_reg); }

        FILE* f_stat = fopen(files_stats[i], "w");
        if (f_stat) {
            fprintf(f_stat, "cycles %d\n", cores[i].cycles);
            fprintf(f_stat, "instructions %d\n", cores[i].instructions);
            fprintf(f_stat, "read_hit %d\n", cores[i].read_hits);
            fprintf(f_stat, "write_hit %d\n", cores[i].write_hits);
            fprintf(f_stat, "read_miss %d\n", cores[i].read_misses);
            fprintf(f_stat, "write_miss %d\n", cores[i].write_misses);
            fprintf(f_stat, "decode_stall %d\n", cores[i].decode_stalls);
            fprintf(f_stat, "mem_stall %d\n", cores[i].mem_stalls);
            fclose(f_stat);
        }

        FILE* f_ds = fopen(files_dsram[i], "w");
        FILE* f_ts = fopen(files_tsram[i], "w");
        for (int s = 0; s < CACHE_SETS; s++) {
            int ts_val = (cores[i].tsram[s].state << 12) | (cores[i].tsram[s].tag & 0xFFF);
            if (f_ts) fprintf(f_ts, "%08X\n", ts_val);
            if (f_ds) for (int w = 0; w < BLOCK_SIZE; w++) fprintf(f_ds, "%08X\n", cores[i].dsram[s].data[w]);
        }
        if (f_ds) fclose(f_ds); if (f_ts) fclose(f_ts);
        if (fp_trace[i]) fclose(fp_trace[i]);
    }
    if (fp_bustrace) fclose(fp_bustrace);
}

int main(int argc, char** argv) {
    // We expect 28 total items: argv[0] is the program name, argv[1]...argv[27] are files.
    if (argc >= 28) {
        // --- INPUT FILES ---
        files_imem[0] = argv[2];  // imem0.txt
        files_imem[1] = argv[3];  // imem1.txt
        files_imem[2] = argv[4];  // imem2.txt
        files_imem[3] = argv[5];  // imem3.txt
        file_memin = argv[6];  // memin.txt

        // --- OUTPUT FILES ---
        file_memout = argv[7];  // memout.txt

        // Register Output
        files_regout[0] = argv[8];  // regout0.txt
        files_regout[1] = argv[9];  // regout1.txt
        files_regout[2] = argv[10];  // regout2.txt
        files_regout[3] = argv[11]; // regout3.txt

        // Core Traces
        files_trace[0] = argv[12]; // core0trace.txt
        files_trace[1] = argv[13]; // core1trace.txt
        files_trace[2] = argv[14]; // core2trace.txt
        files_trace[3] = argv[15]; // core3trace.txt

        // Bus Trace
        file_bustrace = argv[16]; // bustrace.txt

        // DSRAM (Data Cache)
        files_dsram[0] = argv[17]; // dsram0.txt
        files_dsram[1] = argv[18]; // dsram1.txt
        files_dsram[2] = argv[19]; // dsram2.txt
        files_dsram[3] = argv[20]; // dsram3.txt

        // TSRAM (Tag Cache)
        files_tsram[0] = argv[21]; // tsram0.txt
        files_tsram[1] = argv[22]; // tsram1.txt
        files_tsram[2] = argv[23]; // tsram2.txt
        files_tsram[3] = argv[24]; // tsram3.txt

        // Stats
        files_stats[0] = argv[25]; // stats0.txt
        files_stats[1] = argv[26]; // stats1.txt
        files_stats[2] = argv[27]; // stats2.txt
        files_stats[3] = argv[28]; // stats3.txt
    }
    else if (argc > 1) {
        printf("Error: Expected 27 arguments, got %d. Using default filenames.\n", argc - 1);
        // Optional: return 1; to force exit if args are wrong
    }

    // Initialize Cores
    for (int i = 0; i < NUM_CORES; i++) {
        cores[i].id = i; cores[i].pc = 0;
        load_file(files_imem[i], cores[i].imem, IMEM_SIZE);
        fp_trace[i] = fopen(files_trace[i], "w");
    }
    // Initialize Memory & Bus Trace
    load_file(file_memin, main_mem, MEM_SIZE);
    fp_bustrace = fopen(file_bustrace, "w");

    // --- Main Simulation Loop ---
    bool running = true;
    int current_bus_latency = MEM_ACCESS_LATENCY; // Default to 16

    while (running && global_clock < MAX_CYCLES) {

        // ====================================================================
        // STEP 1: Arbitration (Moved to Top for Correct Timing)
        // Checks requests from the *previous* cycle.
        // ====================================================================
        if (!system_bus.busy) {
            for (int i = 0; i < NUM_CORES; i++) {
                int curr_id = (bus_arbitration_idx + i) % NUM_CORES;
                if (cores[curr_id].needs_bus && global_clock > cores[curr_id].bus_request_cycle ) {
                    // Grant Bus
                    system_bus.busy = true;
                    system_bus.bus_origid = curr_id;
                    system_bus.bus_cmd = cores[curr_id].pending_cmd;
                    system_bus.bus_addr = cores[curr_id].pending_addr;
                    system_bus.bus_data = 0;
                    system_bus.bus_shared = 0;
                    system_bus.timer = 0;
                    system_bus.burst_count = 0;
                    system_bus.requesting_core_id = curr_id;

                    // Reset Latency
                    current_bus_latency = MEM_ACCESS_LATENCY;

                    system_bus.timer = -1; // <--- CHANGE: Start at -1 to ensure full 16 cycle delay

                    // Clear Core Request
                    cores[curr_id].needs_bus = false;

                    // FIX: Update Round-Robin pointer immediately to enforce fairness.
                    // The core that just got the bus becomes the last priority.
                    bus_arbitration_idx = (curr_id + 1) % NUM_CORES;

                    // Bus Trace Log (Command Phase)
                    if (fp_bustrace) {
                        fprintf(fp_bustrace, "%d %X %X %06X %08X %X\n",
                            global_clock, system_bus.bus_origid, system_bus.bus_cmd,
                            system_bus.bus_addr, 0, 0);
                    }
                    break; // Only one grant per cycle
                }
            }
        }

        // ====================================================================
        // STEP 2: Snooping Logic
        // Must happen immediately if the bus just became busy this cycle.
        // ====================================================================
        if (system_bus.busy && system_bus.timer == 0) {
            for (int i = 0; i < NUM_CORES; i++) {
                if (i != system_bus.requesting_core_id) {
                    int result = snoop_core(i, system_bus.bus_addr, system_bus.bus_cmd);

                    if (result >= 1) system_bus.bus_shared = 1;

                    if (result == 2) { // FLUSH DETECTED!
                        system_bus.bus_origid = i; // The snooper becomes the originator of data
                        current_bus_latency = 1;   // Reduced latency for Flush
                    }
                }
            }
        }

        // ====================================================================
        // STEP 3: Step Cores
        // Run pipeline stages. If a miss occurs here, it sets 'needs_bus'
        // which will be seen by the Arbiter in the NEXT cycle.
        // ====================================================================
        int halted_count = 0;
        for (int i = 0; i < NUM_CORES; i++) {
            step_core(i);
            if (cores[i].halted) halted_count++;
        }

        // ====================================================================
        // STEP 4: Bus Data Transfer Logic
        // ====================================================================
        if (system_bus.busy) {
            system_bus.timer++;

            if (system_bus.timer >= current_bus_latency) {

                // For trace purposes, Data Transfer is logged as Flush/Data
                // If it's a normal memory read, the originator is memory (4).
                // If it was a Flush, the originator was set in Step 2.
                int trace_orig = system_bus.bus_origid;
                if (trace_orig == system_bus.requesting_core_id) {
                    trace_orig = 4; // Main Memory
                }

                int block_start_addr = (system_bus.bus_addr >> 3) << 3;
                int current_word_offset = system_bus.burst_count;
                int current_addr = block_start_addr + current_word_offset;

                // Get Data (Main memory is always up to date due to Flush or previous writes)
                system_bus.bus_data = main_mem[current_addr];

                // Update address for trace
                system_bus.bus_addr = current_addr;

                // Write to Requesting Core's Cache
                Core* requester = &cores[system_bus.requesting_core_id];
                int set_idx = GET_INDEX(current_addr);
                requester->dsram[set_idx].data[current_word_offset] = system_bus.bus_data;

                // Trace Output (Data Phase)
                if (fp_bustrace) {
                    fprintf(fp_bustrace, "%d %X %X %06X %08X %X\n",
                        global_clock,
                        trace_orig,
                        BUS_CMD_FLUSH, // Data transfer is logged as type 3 (Flush/Response)
                        system_bus.bus_addr,
                        system_bus.bus_data,
                        system_bus.bus_shared);
                }

                // Check Burst Completion (Block Size = 8)
                if (system_bus.burst_count == BLOCK_SIZE - 1) {
                    int req_tag = GET_TAG(block_start_addr);
                    requester->tsram[set_idx].tag = req_tag;

                    // Update State based on Shared Line
                    if (system_bus.bus_shared) requester->tsram[set_idx].state = MESI_S;
                    else requester->tsram[set_idx].state = MESI_E;

                    // If it was a Write Miss (ReadX), upgrade to Modified immediately
                    if (cores[system_bus.requesting_core_id].pending_cmd == BUS_CMD_READX) {
                        requester->tsram[set_idx].state = MESI_M;
                    }

                    // Release Core
                    requester->stall_timer = 0;

                    // Release Bus
                    system_bus.busy = false;
                    system_bus.bus_cmd = BUS_CMD_NO_CMD;
                }
                else {
                    system_bus.burst_count++;
                }
            }
        }

        // ====================================================================
        // STEP 5: Cycle Update & Termination Check
        // ====================================================================
        global_clock++;
        if (halted_count == NUM_CORES) running = false;
    }

    print_output_files();
    return 0;
}
