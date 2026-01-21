/*
 * Project: Multi-Core Cache Simulator (MIPS-like)
 * File:    core.c
 * * Description:
 * Implements a 5-stage MIPS-style pipeline. The core supports a custom 16-register
 * ISA with integrated L1 Cache support. The pipeline stages are executed in
 * reverse order in the source code to model the behavior of clocked registers.
 * *  */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core.h"

 /**
  * @brief Sign-extends a 12-bit immediate value to 32 bits.
  */
uint32_t sign_extend(uint32_t imm) {
    if (imm & 0x800) {
        return imm | 0xFFFFF000;
    }
    return imm;
}

/* Helper macros for instruction parsing */
#define GET_OPCODE(inst) ((inst >> 24) & 0xFF)
#define GET_RD(inst)     ((inst >> 20) & 0xF)
#define GET_RS(inst)     ((inst >> 16) & 0xF)
#define GET_RT(inst)     ((inst >> 12) & 0xF)
#define GET_IMM(inst)    (inst & 0xFFF)

void core_init(Core* core, int id, const char* imem_path) {
    memset(core, 0, sizeof(Core));
    core->id = id;

    cache_init(&core->l1_cache, id);

    FILE* fp = fopen(imem_path, "r");
    if (fp) {
        int i = 0;
        char line[100];
        while (fgets(line, sizeof(line), fp) && i < 1024) {
            core->instruction_memory[i] = (uint32_t)strtol(line, NULL, 16);
            i++;
        }
        fclose(fp);
    }
    else {
        printf("Error: Could not open IMEM file %s\n", imem_path);
    }
}

/**
 * @brief Stage 5: WriteBack (WB)
 * * Retires instructions and writes results back to the Register File.
 * * Updates 'wb_hazard_rd' to facilitate hazard detection in the Decode stage.
 */
void stage_wb(Core* core) {
    /* 1. HAZARD CLEARING */
    core->wb_hazard_rd = 0;

    if (core->stall) return;
    if (!core->mem_wb.valid) return;

    MEM_WB_Latch* in = &core->mem_wb;
    Opcode op = in->Op;

    /* 2. HALT HANDLING */
    if (op == OP_HALT) {
        core->halted = true;
        return;
    }

    /* 3. WRITE ENABLE LOGIC */
    bool write_enable = true;
    if (op == OP_SW ||
        op == OP_BEQ || op == OP_BNE ||
        op == OP_BLT || op == OP_BGT ||
        op == OP_BLE || op == OP_BGE) {
        write_enable = false;
    }

    /* 4. REGISTER WRITE */
    if (write_enable) {
        uint32_t dest_reg = in->Rd_Index;
        uint32_t write_data;

        if (op == OP_LW) {
            write_data = in->MemData;
        }
        else if (op == OP_JAL) {
            dest_reg = 15; // JAL always writes return address to R15
            write_data = in->PC + 1;
        }
        else {
            write_data = in->ALUOutput;
        }

        // Only registers R2-R15 are writable. R0 is constant 0, R1 is Imm.
        if (dest_reg >= 2 && dest_reg < 16) {
            core->regs[dest_reg] = write_data;
            core->wb_hazard_rd = dest_reg; // Broadcast write to arbiter for hazard detection
        }
    }
}

/**
 * @brief Stage 4: Memory Access (MEM)
 * * Interacts with the L1 Cache for Load (LW) and Store (SW) instructions.
 * * If the cache access results in a miss, the entire pipeline is stalled.
 */
void stage_mem(Core* core, Bus* bus) {
    EX_MEM_Latch* in = &core->ex_mem;
    MEM_WB_Latch* out = &core->mem_wb;
    uint32_t rd_data = core->regs[in->Rd_Index];
    out->valid = false;

    if (!in->valid) return;

    bool mem_busy = false;

    /* 1. CACHE ACCESS LOGIC */
    if (in->Op == OP_LW) {
        if (cache_read(&core->l1_cache, in->ALUOutput, &out->MemData, bus)) {
            mem_busy = false;
        }
        else {
            mem_busy = true;
            core->stats.mem_stalls++;
        }
    }
    else if (in->Op == OP_SW) {
        if (cache_write(&core->l1_cache, in->ALUOutput, rd_data, bus)) {
            mem_busy = false;
        }
        else {
            mem_busy = true;
            core->stats.mem_stalls++;
        }
    }

    if (mem_busy) {
        core->stall = true; // Trigger pipeline-wide stall
        return;
    }

    // Move data to next latch
    out->PC = in->PC;
    out->ALUOutput = in->ALUOutput;
    out->Rd_Index = in->Rd_Index;
    out->Op = in->Op;
    out->valid = true;
}

/**
 * @brief Stage 3: Execute (EX)
 * * Performs ALU calculations based on the opcode decoded in the previous stage.
 */
void stage_ex(Core* core) {
    ID_EX_Latch* in = &core->id_ex;
    EX_MEM_Latch* out = &core->ex_mem;
    if (core->stall) return;
    out->valid = false;
    if (!in->valid) return;

    out->PC = in->PC;
    out->Rd_Index = in->Rd_Index;
    out->B = in->B;
    out->Op = in->Op;

    /* 1. ALU OPERATIONS */
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
        out->ALUOutput = in->A + in->B; // Effective address calculation
        break;
    case OP_JAL:
        out->ALUOutput = in->PC + 1;
        break;
    default: out->ALUOutput = 0; break;
    }
    out->valid = true;
}

/**
 * @brief Stage 2: Decode (ID)
 * * Decodes instructions, reads the Register File, and detects data hazards.
 * * Branches are resolved early here to minimize branch penalty.
 */
void stage_decode(Core* core) {
    IF_ID_Latch* in = &core->if_id;
    ID_EX_Latch* out = &core->id_ex;

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
    if (op == OP_HALT) core->halt_detected = true;

    uint32_t rs = GET_RS(inst);
    uint32_t rt = GET_RT(inst);
    uint32_t rd = GET_RD(inst);
    uint32_t imm_sext = sign_extend(GET_IMM(inst));

    bool hazard = false;

    /* 1. DATA HAZARD DETECTION (RAW)
     * We check if any instruction currently in EX, MEM, or WB stages
     * is planning to write to the registers we need to read now.
     */
#define CHECK_HAZARD(reg_idx) \
        if (reg_idx >= 2) { \
            if (core->id_ex.valid && core->id_ex.Rd_Index == reg_idx) hazard = true; \
            if (core->ex_mem.valid && core->ex_mem.Rd_Index == reg_idx) hazard = true; \
            if (core->mem_wb.valid && core->mem_wb.Rd_Index == reg_idx) hazard = true; \
            if (core->wb_hazard_rd == reg_idx) hazard = true; \
        }

    CHECK_HAZARD(rs);
    if (op != OP_JAL) { CHECK_HAZARD(rt); }
    if (op == OP_SW || op == OP_BEQ || op == OP_BNE || op == OP_BLT || op == OP_BGT || op == OP_BLE || op == OP_BGE) {
        CHECK_HAZARD(rd);
    }

    if (hazard) {
        out->valid = false;
        core->stats.decode_stalls++;
        return;
    }

    /* 2. OPERAND FETCH & BRANCH RESOLUTION */
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
        if (op == OP_JAL) out->Rd_Index = 15;
        uint32_t target = (rd == 1) ? imm_sext : core->regs[rd];
        core->branch_pending = true;
        core->branch_target = target & 0x3FF; // Wrap to IMEM size
    }

    out->PC = in->PC;
    out->Op = op;
    out->Rd_Index = rd;
    out->A = val_rs;
    out->B = val_rt;
    out->Imm = imm_sext;
    out->valid = true;
}

/**
 * @brief Stage 1: Instruction Fetch (IF)
 * * Fetches the next instruction from instruction memory (IMEM).
 * * Manages the Program Counter (PC) and handles branch flushes/updates.
 */
void stage_fetch(Core* core) {
    if (core->stall) return;
    if (core->halt_detected) {
        core->if_id.Instruction = 0;
        core->if_id.PC = 0;
        return;
    }

    /* 1. HAZARD CHECK (FETCH BLOCKING)
     * Ensures we don't fetch a new instruction if the Decode stage is stalled.
     */
    bool decode_stall = false;
    if (core->if_id.Instruction != 0) {
        uint32_t inst = core->if_id.Instruction;
        uint32_t rs = GET_RS(inst); uint32_t rt = GET_RT(inst); uint32_t rd = GET_RD(inst);
        Opcode op = GET_OPCODE(inst);

#define CHECK_HAZARD_FETCH(reg_idx) \
        if (reg_idx >= 2) { \
            if (core->id_ex.valid && core->id_ex.Rd_Index == reg_idx) { if (core->id_ex.PC != core->if_id.PC) decode_stall = true; } \
            if (core->ex_mem.valid && core->ex_mem.Rd_Index == reg_idx) decode_stall = true; \
            if (core->mem_wb.valid && core->mem_wb.Rd_Index == reg_idx) decode_stall = true; \
            if (core->wb_hazard_rd == reg_idx) decode_stall = true; \
        }
        CHECK_HAZARD_FETCH(rs);
        if (op != OP_JAL) { CHECK_HAZARD_FETCH(rt); }
        if (op == OP_SW || op == OP_BEQ || op == OP_BNE || op == OP_BLT || op == OP_BGT || op == OP_BLE || op == OP_BGE) {
            CHECK_HAZARD_FETCH(rd);
        }
    }

    if (decode_stall) return;

    /* 2. MEMORY ACCESS & PC UPDATE */
    if (core->pc < 1024) {
        core->if_id.Instruction = core->instruction_memory[core->pc];
        core->if_id.PC = core->pc;
        if (core->branch_pending) {
            core->pc = core->branch_target;
            core->branch_pending = false;
        }
        else {
            core->pc++;
        }
    }
    else {
        core->if_id.Instruction = 0;
    }
}

/**
 * @brief Advances the core clock and executes all stages.
 * * Stages are called in reverse order to ensure that each stage uses the data
 * produced by the previous stage in the *previous* clock cycle.
 */
void core_cycle(Core* core, Bus* bus) {
    if (core->halted) return;

    // Save state for trace reporting
    memcpy(core->trace_regs, core->regs, sizeof(core->regs));
    core->stats.cycles++;
    core->stall = false;

    // Execute pipeline in reverse order
    stage_wb(core);
    stage_mem(core, bus);
    stage_ex(core);
    stage_decode(core);
    stage_fetch(core);

    // Instruction count logic (accounting for valid WB retirement)
    if (core->mem_wb.valid) core->stats.instructions++;
}