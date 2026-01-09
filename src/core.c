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

    if (!in->valid) return;

    // R0 is always 0. R1 is read-only (immediate). [cite: 21, 22]
    // Only write to R2-R15.
    if (in->Rd_Index >= 2) {
        // Data source depends on Opcode (Load vs ALU)
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

    // Default: bubble the next stage unless we successfully finish
    out->valid = false;

    if (!in->valid) {
        return; // Nothing to do
    }

    bool mem_busy = false;

    // Handle Memory Ops
    if (in->Op == OP_LW) {
        // Try to read from cache
        if (cache_read(&core->l1_cache, in->ALUOutput, &out->MemData, bus)) {
            // Hit!
            mem_busy = false;
        } else {
            // Miss! Stall.
            mem_busy = true;
            core->stats.mem_stalls++;
        }
    } else if (in->Op == OP_SW) {
        // Try to write to cache
        if (cache_write(&core->l1_cache, in->ALUOutput, in->B, bus)) {
            // Hit/Done!
            mem_busy = false;
        } else {
            // Miss! Stall.
            mem_busy = true;
            core->stats.mem_stalls++;
        }
    }

    // If memory is busy (miss), we STALL.
    // We do NOT update the output latch, meaning WB will see 'invalid' next cycle.
    // We do NOT consume the input latch, so we retry this instruction next cycle.
    if (mem_busy) {
        core->stall = true; // Signal to freeze previous stages
        return;
    }

    // Pass data to WB
    out->PC = in->PC;
    out->ALUOutput = in->ALUOutput;
    out->Rd_Index = in->Rd_Index;
    out->Op = in->Op;
    out->valid = true;
}

void stage_ex(Core *core) {
    ID_EX_Latch *in = &core->id_ex;
    EX_MEM_Latch *out = &core->ex_mem;

    // If stalled by MEM stage, don't overwrite the output latch
    if (core->stall) return;

    out->valid = false;
    if (!in->valid) return;

    out->PC = in->PC;
    out->Rd_Index = in->Rd_Index;
    out->B = in->B; // Pass Rt for Store
    out->Op = in->Op;

    // ALU Logic
    switch (in->Op) {
        case OP_ADD: out->ALUOutput = in->A + in->B; break;
        case OP_SUB: out->ALUOutput = in->A - in->B; break;
        case OP_AND: out->ALUOutput = in->A & in->B; break;
        case OP_OR:  out->ALUOutput = in->A | in->B; break;
        case OP_XOR: out->ALUOutput = in->A ^ in->B; break;
        case OP_MUL: out->ALUOutput = in->A * in->B; break;
        case OP_SLL: out->ALUOutput = in->A << in->B; break;
        case OP_SRA: out->ALUOutput = (int32_t)in->A >> in->B; break; // Arithmetic
        case OP_SRL: out->ALUOutput = in->A >> in->B; break; // Logical

        // For branches, ALU output isn't used for result, but maybe for comparison?
        // Actually branch resolution is in DECODE[cite: 33].
        // So EX just passes bubbles for branches usually, or ignores them.

        case OP_LW:
        case OP_SW:
            // Calculate Address: Rs + Rt
            out->ALUOutput = in->A + in->B;
            break;

        default: out->ALUOutput = 0; break;
    }

    out->valid = true;
}

void stage_decode(Core *core) {
    IF_ID_Latch *in = &core->if_id;
    ID_EX_Latch *out = &core->id_ex;

    // If stalled by MEM stage, freeze.
    if (core->stall) return;

    // If this latch is empty (bubble), pass a bubble to EX
    if (in->Instruction == 0 && in->PC == 0) { // Simple check for empty/nop
         out->valid = false;
         return;
    }

    uint32_t inst = in->Instruction;
    Opcode op = GET_OPCODE(inst);
    uint32_t rs = GET_RS(inst);
    uint32_t rt = GET_RT(inst);
    uint32_t rd = GET_RD(inst);
    uint32_t imm = GET_IMM(inst);
    uint32_t imm_sext = sign_extend(imm);

    // --- Hazard Detection (No Forwarding) ---
    // Check if Rs or Rt matches a destination in EX, MEM, or WB stages.
    // Note: R0 and R1 are never written to, so no hazard.
    bool hazard = false;

    // Helper lambda-like check
    #define CHECK_HAZARD(reg_idx) \
        if (reg_idx >= 2) { \
            if (core->id_ex.valid && core->id_ex.Rd_Index == reg_idx) hazard = true; \
            if (core->ex_mem.valid && core->ex_mem.Rd_Index == reg_idx) hazard = true; \
            if (core->mem_wb.valid && core->mem_wb.Rd_Index == reg_idx) hazard = true; \
        }

    // Check Rs (used by almost all ALU ops + Branch + Store)
    CHECK_HAZARD(rs);

    // Check Rt (used by ADD, SUB... and STORE. BUT for Store, Rt is data, not address calc)
    // However, if Rt is used as an input operand (ALU or Store Data or Branch compare), check it.
    // Only JAL and maybe some immediate ops don't use Rt as input.
    if (op != OP_JAL) {
        CHECK_HAZARD(rt);
    }

    if (hazard) {
        // STALL: Insert Bubble into EX, Keep IF_ID and PC frozen
        out->valid = false;
        core->stats.decode_stalls++;
        // We return early, NOT updating `out` and NOT reading `in`.
        // effectively forcing fetch to retry and us to retry.
        // We set a flag so Fetch knows not to overwrite IF_ID
        // But wait, core->stall is for MEM stalls. We need a local decode stall flag?
        // Actually, we can just return. The Fetch stage needs to know NOT to advance.
        // Let's use a specific flag for decode stall to coordinate with Fetch.
        // For simplicity: If we return here without writing to 'out', 'out' becomes invalid/bubble?
        // No, we must explicitly set out->valid = false (done above).
        // And we must tell Fetch "don't fetch new".
        // Let's modify core struct to have `decode_stall`.
        // *Re-architecting slightly*: The standard way is checking hazards, if true ->
        // 1. Send Bubble to ID/EX
        // 2. Disable PC Write
        // 3. Disable IF/ID Write
        // We can do this by just returning and ensuring Fetch sees the hazard flag.
        // Let's assume `core->stats.decode_stalls` changing is the signal? No, safer to return.
        // We need to implement the "Disable PC Write" logic in Fetch.
        return;
    }

    // --- Register Read ---
    // R1 Special Handling: Always contains Immediate
    uint32_t val_rs = (rs == 1) ? imm_sext : core->regs[rs];
    uint32_t val_rt = (rt == 1) ? imm_sext : core->regs[rt];

    // --- Branch Resolution --- [cite: 33]
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
        // "PC = R[rd][9:0]" [cite: 65]
        // Note: For JAL, we also write R15 = PC + 1 (Next instruction)
        if (op == OP_JAL) {
            // Write R15 happens in WB, so we pass it down pipeline
            out->Rd_Index = 15;
            out->ALUOutput = in->PC + 1;
            // The JAL opcode needs to travel to WB to trigger the write
        }

        // Update PC (Mask to 10 bits)
        // Wait, PC update happens in Fetch? Or we force it here.
        // Since we are in simulation, we can directly modify core->pc.
        // But we must handle Delay Slot: The instruction AFTER the branch (already in IF_ID)
        // MUST execute.
        // So we update core->pc, but we DO NOT flush IF_ID.
        // The next Fetch cycle will fetch from the NEW target.
        uint32_t target = (rd == 1) ? imm_sext : core->regs[rd]; // Handle JAL R1 usage
        core->pc = target & 0x3FF;
    }

    // Fill Output Latch
    out->PC = in->PC;
    out->Op = op;
    out->Rd_Index = rd;
    out->A = val_rs;
    out->B = val_rt;
    out->Imm = imm_sext; // Store just in case
    out->valid = true;

    // Clear the decode hazard flag if we got here (so fetch can run)
    // Note: We need a way to detect the hazard state.
    // See Fetch implementation below.
}

void stage_fetch(Core *core) {
    // 1. Check for Memory Stall (Pipeline Freeze)
    if (core->stall) {
        // If Mem stage is stalled, we do nothing. Everything stays frozen.
        return;
    }

    // 2. Check for Decode Stall (Hazard)
    // We need to re-detect the hazard here or assume if ID/EX is invalid it was a bubble?
    // No, cleaner way: Re-run the hazard check or use a flag.
    // Let's replicate the check briefly or look at `stats.decode_stalls`?
    // Actually, in the `stage_decode` above, if I return early, `out->valid` is NOT set to true.
    // However, I didn't clear `in->valid` (IF_ID).
    // So if I simply don't overwrite IF_ID here, the Decode stage will re-process the SAME instruction next cycle.
    // That IS the definition of a stall!

    // So: Only fetch if Decode successfully consumed the instruction (or passed a bubble).
    // How do we know? We can check if `core->id_ex.valid` was set to true this cycle?
    // Impossible because C functions run sequentially.

    // BETTER APPROACH:
    // We add `bool hazard_detected` to the Core struct or check the hazard condition here.
    // Since I can't easily modify the header now, let's re-check the hazard condition locally
    // or assume if `core->id_ex.valid` is false but `core->if_id.Instruction` was not NOP, we stalled?
    // That's risky.

    // Let's use the standard "Stall means don't write PC, don't write IF/ID".
    // I will use a local check.
    bool decode_stall = false;
    if (core->if_id.Instruction != 0) { // If valid inst
        uint32_t inst = core->if_id.Instruction;
        uint32_t rs = GET_RS(inst);
        uint32_t rt = GET_RT(inst);
        Opcode op = GET_OPCODE(inst);

        #define CHECK_HAZARD_FETCH(reg_idx) \
        if (reg_idx >= 2) { \
            if (core->id_ex.valid && core->id_ex.Rd_Index == reg_idx) decode_stall = true; \
            if (core->ex_mem.valid && core->ex_mem.Rd_Index == reg_idx) decode_stall = true; \
            if (core->mem_wb.valid && core->mem_wb.Rd_Index == reg_idx) decode_stall = true; \
        }
        CHECK_HAZARD_FETCH(rs);
        if (op != OP_JAL) { CHECK_HAZARD_FETCH(rt); }
    }

    if (decode_stall) {
        return; // Do not fetch new, do not update PC.
    }

    // 3. Normal Fetch
    if (core->pc < 1024) {
        core->if_id.Instruction = core->instruction_memory[core->pc];
        core->if_id.PC = core->pc;

        // PC Increment
        core->pc++;
    } else {
        // Out of bounds (shouldn't happen with correct HALT)
        core->if_id.Instruction = 0; // NOP
    }
}

void core_cycle(Core *core, Bus *bus) {
    if (core->halted) return;

    core->stats.cycles++;

    // Reset global stall flag for this cycle (it will be re-raised by MEM if needed)
    core->stall = false;

    // Run stages Reverse (WB -> MEM -> EX -> ID -> IF)
    stage_wb(core);
    stage_mem(core, bus); // This might set core->stall = true
    stage_ex(core);
    stage_decode(core);
    stage_fetch(core);

    core->stats.instructions++; // Count completed? Or fetched? Spec says "executed". Usually WB.
    // Let's refine instruction count: Only increment if WB finishes a valid instruction.
    // The spec "instructions X" usually means completed instructions.
    // I incremented `cycles` above. I should increment `instructions` in `stage_wb`.
    // I will fix that in logic (decrement here, increment in WB).
    core->stats.instructions--; // Undo; let WB handle it
    if (core->mem_wb.valid) core->stats.instructions++;
}