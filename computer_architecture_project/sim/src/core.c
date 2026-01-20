/*
 * Project: Multi-Core Cache Simulator (MIPS-like)
 * File:    core.c
 * Author:
 * ID:
 * Date:    11/11/2024
 *
 * Description:
 * Implements the 5-stage MIPS pipeline (Fetch, Decode, Execute, Memory, WriteBack).
 * Handles hazard detection, stalling, forwarding (conceptually), and interfacing
 * with the L1 cache.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core.h"

uint32_t sign_extend(uint32_t imm) {
    if (imm & 0x800) {
        return imm | 0xFFFFF000;
    }
    return imm;
}

#define GET_OPCODE(inst) ((inst >> 24) & 0xFF)
#define GET_RD(inst)     ((inst >> 20) & 0xF)
#define GET_RS(inst)     ((inst >> 16) & 0xF)
#define GET_RT(inst)     ((inst >> 12) & 0xF)
#define GET_IMM(inst)    (inst & 0xFFF)

void core_init(Core *core, int id, const char *imem_path) {
    memset(core, 0, sizeof(Core));
    core->id = id;

    cache_init(&core->l1_cache, id);

    FILE *fp = fopen(imem_path, "r");
    if (fp) {
        int i = 0;
        char line[100];
        while (fgets(line, sizeof(line), fp) && i < 1024) {
            core->instruction_memory[i] = (uint32_t)strtol(line, NULL, 16);
            i++;
        }
        fclose(fp);
    } else {
        printf("Error: Could not open IMEM file %s\n", imem_path);
    }
}

void stage_wb(Core *core) {
    /*
     * 1. HAZARD CLEARING
     * Reset the hazard tracker at the start of the cycle.
     * This clears the flag from the previous cycle so we don't stall unnecessarily.
     */
    core->wb_hazard_rd = 0;

    if (core->stall) return;
    if (!core->mem_wb.valid) return;

    MEM_WB_Latch *in = &core->mem_wb;
    Opcode op = in->Op;

    /*
     * 2. HALT HANDLING
     * If we reach WriteBack with a HALT instruction, the core stops.
     */
    if (op == OP_HALT) {
        core->halted = true;
        return;
    }

    /*
     * 3. WRITE ENABLE LOGIC
     * Determine if this instruction SHOULD write to a register.
     * By default, ALU ops and Loads write. Stores and Branches do NOT.
     */
    bool write_enable = true;

    if (op == OP_SW ||
        op == OP_BEQ || op == OP_BNE ||
        op == OP_BLT || op == OP_BGT ||
        op == OP_BLE || op == OP_BGE ) {
        write_enable = false;
        }

    /*
     * 4. REGISTER WRITE
     * Perform the actual write to the register file.
     */
    if (write_enable) {
        uint32_t dest_reg = in->Rd_Index;
        uint32_t write_data;

        // A. Determine Data Source
        if (op == OP_LW) {
            write_data = in->MemData;
        } else if (op == OP_JAL) {
            // JAL is special: It writes PC+1 to R15
            dest_reg = 15;
            write_data = in->PC + 1;
        } else {
            // Standard ALU arithmetic (Add, Sub, etc.)
            write_data = in->ALUOutput;
        }

        // B. Write to Register File (Only R2-R15 are writable)
        if (dest_reg >= 2 && dest_reg < 16) {
            core->regs[dest_reg] = write_data;

            // C. Publish the Hazard
            // We tell the Decode stage: "I am writing to this register NOW."
            // This allows the Decode stage to stall if it tries to read this specific reg.
            core->wb_hazard_rd = dest_reg;
        }
    }
}

void stage_mem(Core *core, Bus *bus) {
    EX_MEM_Latch *in = &core->ex_mem;
    MEM_WB_Latch *out = &core->mem_wb;
    uint32_t rd_data = core->regs[in->Rd_Index];
    out->valid = false;
    if (!in->valid) {
        return;
    }
    bool mem_busy = false;

    /*
     * 1. CACHE ACCESS
     * Attempt to read or write to the L1 Cache.
     * If the cache returns false (miss/busy), we stall the pipeline.
     */
    if (in->Op == OP_LW) {
        if (cache_read(&core->l1_cache, in->ALUOutput, &out->MemData, bus)) {
            mem_busy = false;
        } else {
            mem_busy = true;
            core->stats.mem_stalls++;
        }
    } else if (in->Op == OP_SW) {
        if (cache_write(&core->l1_cache, in->ALUOutput, rd_data, bus)) {
            mem_busy = false;
        } else {
            mem_busy = true;
            core->stats.mem_stalls++;
        }
    }
    if (mem_busy) {
        core->stall = true;
        return;
    }
    out->PC = in->PC;
    out->ALUOutput = in->ALUOutput;
    out->Rd_Index = in->Rd_Index;
    out->Op = in->Op;
    out->valid = true;
}

void stage_ex(Core *core) {
    ID_EX_Latch *in = &core->id_ex;
    EX_MEM_Latch *out = &core->ex_mem;
    if (core->stall) return;
    out->valid = false;
    if (!in->valid) return;
    out->PC = in->PC;
    out->Rd_Index = in->Rd_Index;
    out->B = in->B; // Pass Rt for Store
    out->Op = in->Op;

    /*
     * 1. ALU OPERATIONS
     * Perform the arithmetic or logic operation based on the opcode.
     */
    switch (in->Op) {
        case OP_ADD: out->ALUOutput = in->A + in->B; break;
        case OP_SUB: out->ALUOutput = in->A - in->B; break;
        case OP_AND: out->ALUOutput = in->A & in->B; break;
        case OP_OR:  out->ALUOutput = in->A | in->B; break;
        case OP_XOR: out->ALUOutput = in->A ^ in->B; break;
        case OP_MUL: out->ALUOutput = in->A * in->B; break;
        case OP_SLL: out->ALUOutput = in->A << in->B; break;
        case OP_SRA: out->ALUOutput = (int32_t)in->A >> in->B; break;
        case OP_SRL: out->ALUOutput = in->A >> in->B; break;
        case OP_LW:
        case OP_SW:
            out->ALUOutput = in->A + in->B;
            break;
        case OP_JAL:
            out->ALUOutput = in->PC + 1;
            break;
        default: out->ALUOutput = 0; break;
    }
    out->valid = true;
}

void stage_decode(Core *core) {
    IF_ID_Latch *in = &core->if_id;
    ID_EX_Latch *out = &core->id_ex;
    if (core->stall) return;
    if (core->halt_detected) {
        out->valid = false;
        return;
    }
    if (in->Instruction == 0 && in->PC == 0) {
        out->valid = false;
        return;
    }
    uint32_t inst = in->Instruction;
    Opcode op = GET_OPCODE(inst);
    if (op == OP_HALT) {
        core->halt_detected = true;
    }
    uint32_t rs = GET_RS(inst);
    uint32_t rt = GET_RT(inst);
    uint32_t rd = GET_RD(inst);
    uint32_t imm = GET_IMM(inst);
    uint32_t imm_sext = sign_extend(imm);
    bool hazard = false;

    /*
     * 1. HAZARD DETECTION
     * Check if any previous stage is writing to a register we need.
     * If so, stall (insert bubble).
     */
    #define CHECK_HAZARD(reg_idx) \
        if (reg_idx >= 2) { \
            if (core->id_ex.valid && core->id_ex.Rd_Index == reg_idx) hazard = true; \
            if (core->ex_mem.valid && core->ex_mem.Rd_Index == reg_idx) hazard = true; \
            if (core->mem_wb.valid && core->mem_wb.Rd_Index == reg_idx) hazard = true; \
            if (core->wb_hazard_rd == reg_idx) hazard = true; \
        }
    CHECK_HAZARD(rs);
    if (op != OP_JAL) {
        CHECK_HAZARD(rt);
    }
    if (op == OP_SW ||
        op == OP_BEQ || op == OP_BNE ||
        op == OP_BLT || op == OP_BGT ||
        op == OP_BLE || op == OP_BGE) {
        CHECK_HAZARD(rd);
    }
    if (hazard) {
        out->valid = false;
        core->stats.decode_stalls++;
        return;
    }

    /*
     * 2. BRANCH RESOLUTION
     * Resolve branches in the Decode stage.
     * If taken, update PC and flush Fetch stage (handled in Fetch).
     */
    uint32_t val_rs = (rs == 1) ? imm_sext : core->regs[rs];
    uint32_t val_rt = (rt == 1) ? imm_sext : core->regs[rt];
    bool branch_taken = false;
    switch (op) {
        case OP_BEQ: branch_taken = (val_rs == val_rt); break;
        case OP_BNE: branch_taken = (val_rs != val_rt); break;
        case OP_BLT: branch_taken = ((int32_t)val_rs < (int32_t)val_rt); break;
        case OP_BGT: branch_taken = ((int32_t)val_rs > (int32_t)val_rt); break;
        case OP_BLE: branch_taken = ((int32_t)val_rs <= (int32_t)val_rt); break;
        case OP_BGE: branch_taken = ((int32_t)val_rs >= (int32_t)val_rt); break;
        case OP_JAL: branch_taken = true; break;
        default: break;
    }
    if (branch_taken) {
        if (op == OP_JAL) {
            out->Rd_Index = 15;
        }
        uint32_t target = (rd == 1) ? imm_sext : core->regs[rd];
        core->branch_pending = true;
        core->branch_target = target & 0x3FF;
    }
    out->PC = in->PC;
    out->Op = op;
    out->Rd_Index = rd;
    out->A = val_rs;
    out->B = val_rt;
    out->Imm = imm_sext;
    out->valid = true;
}

void stage_fetch(Core *core) {
    if (core->stall) return;
    if (core->halt_detected ) {
        core->if_id.Instruction = 0;
        core->if_id.PC = 0;
        return;
    }
    bool decode_stall = false;

    /*
     * 1. HAZARD CHECK (FETCH)
     * Even though Decode handles hazards, Fetch needs to know if it should
     * stop fetching new instructions to avoid overwriting the IF/ID latch
     * if Decode is stalled.
     */
    if (core->if_id.Instruction != 0) {
        uint32_t inst = core->if_id.Instruction;
        uint32_t rs = GET_RS(inst);
        uint32_t rt = GET_RT(inst);
        uint32_t rd = GET_RD(inst);
        Opcode op = GET_OPCODE(inst);
        #define CHECK_HAZARD_FETCH(reg_idx) \
        if (reg_idx >= 2) { \
            if (core->id_ex.valid && core->id_ex.Rd_Index == reg_idx) { \
                if (core->id_ex.PC != core->if_id.PC) decode_stall = true; \
            } \
            if (core->ex_mem.valid && core->ex_mem.Rd_Index == reg_idx) decode_stall = true; \
            if (core->mem_wb.valid && core->mem_wb.Rd_Index == reg_idx) decode_stall = true; \
            if (core->wb_hazard_rd == reg_idx) decode_stall = true; \
        }
        CHECK_HAZARD_FETCH(rs);
        if (op != OP_JAL) { CHECK_HAZARD_FETCH(rt); }
        if (op == OP_SW ||
        op == OP_BEQ || op == OP_BNE ||
        op == OP_BLT || op == OP_BGT ||
        op == OP_BLE || op == OP_BGE) {
            CHECK_HAZARD_FETCH(rd);
        }
    }
    if (decode_stall) {
        return;
    }

    /*
     * 2. INSTRUCTION FETCH
     * Fetch instruction from IMEM at current PC.
     * Handle branch targets if a branch was taken in Decode.
     */
    if (core->pc < 1024) {
        core->if_id.Instruction = core->instruction_memory[core->pc];
        core->if_id.PC = core->pc;
        if (core->branch_pending) {
            core->pc = core->branch_target;
            core->branch_pending = false;
        } else {
            core->pc++;
        }
    } else {
        core->if_id.Instruction = 0;
    }
}

void core_cycle(Core *core, Bus *bus) {
    if (core->halted) return;
    memcpy(core->trace_regs, core->regs, sizeof(core->regs));
    core->stats.cycles++;
    core->stall = false;
    stage_wb(core);
    stage_mem(core, bus);
    stage_ex(core);
    stage_decode(core);
    stage_fetch(core);
    core->stats.instructions++;
    core->stats.instructions--;
    if (core->mem_wb.valid) core->stats.instructions++;
}
