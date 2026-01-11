#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core.h"

// --- Helper Functions ---

// Sign Extension 12-bit -> 32-bit
uint32_t sign_extend(uint32_t imm) {
    if (imm & 0x800) { // If bit 11 is 1 (negative)
        return imm | 0xFFFFF000;
    }
    return imm;
}

// Extract fields from instruction
#define GET_OPCODE(inst) ((inst >> 24) & 0xFF)
#define GET_RD(inst)     ((inst >> 20) & 0xF)
#define GET_RS(inst)     ((inst >> 16) & 0xF)
#define GET_RT(inst)     ((inst >> 12) & 0xF)
#define GET_IMM(inst)    (inst & 0xFFF)

void core_init(Core *core, int id, const char *imem_path) {
    memset(core, 0, sizeof(Core));
    core->id = id;

    // Initialize Cache
    cache_init(&core->l1_cache, id);

    // Load IMEM from file
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

// --- Pipeline Stages ---

void stage_wb(Core *core) {
    MEM_WB_Latch *in = &core->mem_wb;

    // 1. Reset the hazard tracker at the start of the stage
    core->wb_hazard_rd = 0;

    if (!in->valid) return;

    // 2. If this is a valid write to R2-R15, publish the hazard
    if (in->Rd_Index >= 2) {
        core->wb_hazard_rd = in->Rd_Index;

        // --- Existing Write Logic ---
        uint32_t write_data;
        if (in->Op == OP_LW) {
            write_data = in->MemData;
        } else {
            write_data = in->ALUOutput;
        }
        core->regs[in->Rd_Index] = write_data;
    }

    if (in->Op == OP_HALT) {
        core->halted = true;
    }
}

void stage_mem(Core *core, Bus *bus) {
    EX_MEM_Latch *in = &core->ex_mem;
    MEM_WB_Latch *out = &core->mem_wb;
    out->valid = false;
    if (!in->valid) {
        return;
    }
    bool mem_busy = false;
    if (in->Op == OP_LW) {
        if (cache_read(&core->l1_cache, in->ALUOutput, &out->MemData, bus)) {
            mem_busy = false;
        } else {
            mem_busy = true;
            core->stats.mem_stalls++;
        }
    } else if (in->Op == OP_SW) {
        if (cache_write(&core->l1_cache, in->ALUOutput, in->B, bus)) {
            mem_busy = false;
        } else {
            mem_busy = true;
            core->stats.mem_stalls++;
        }
    }
    if (mem_busy) {
        core->stall = true; // Signal to freeze previous stages
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
    if (hazard) {
        out->valid = false;
        core->stats.decode_stalls++;
        return;
    }
    uint32_t val_rs = (rs == 1) ? imm_sext : core->regs[rs];
    uint32_t val_rt = (rt == 1) ? imm_sext : core->regs[rt];
    bool branch_taken = false;
    switch (op) {
        case OP_BEQ: branch_taken = (val_rs == val_rt); break;
        case OP_BNE: branch_taken = (val_rs != val_rt); break;
        case OP_BLT: branch_taken = ((int32_t)val_rs < (int32_t)val_rt); break; // Signed? Assume yes for standard behavior
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
    // bool is_halt_in_decode = (core->id_ex.valid && core->id_ex.Op == OP_HALT); && !is_halt_in_decode
    if (core->halt_detected ) {
        core->if_id.Instruction = 0; // Bubble
        core->if_id.PC = 0;          // Bubble
        return;
    }
    bool decode_stall = false;
    if (core->if_id.Instruction != 0) {
        uint32_t inst = core->if_id.Instruction;
        uint32_t rs = GET_RS(inst);
        uint32_t rt = GET_RT(inst);
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
    }
    if (decode_stall) {
        return;
    }
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