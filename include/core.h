#ifndef CORE_H
#define CORE_H

#include "global.h"
#include "cache.h"
#include "bus.h"

// --- Pipeline Latch Structures ---
// These hold the data moving between stages

typedef struct {
    uint32_t PC;
    uint32_t Instruction;
} IF_ID_Latch;

typedef struct {
    uint32_t PC;
    uint32_t A;           // Rs value
    uint32_t B;           // Rt value
    uint32_t Imm;         // Immediate (Sign Extended)
    uint32_t Rd_Index;    // Destination Register Index
    uint32_t Rt_Index;    // For store/branch logic
    uint32_t Rs_Index;    // For hazard checking
    Opcode   Op;
    bool     valid;       // Is this a real instruction or bubble?
} ID_EX_Latch;

typedef struct {
    uint32_t PC;
    uint32_t ALUOutput;   // Calculation result
    uint32_t B;           // Store value (Rt)
    uint32_t Rd_Index;
    Opcode   Op;
    bool     valid;
} EX_MEM_Latch;

typedef struct {
    uint32_t PC;
    uint32_t MemData;     // Data read from memory
    uint32_t ALUOutput;   // Pass-through result
    uint32_t Rd_Index;
    Opcode   Op;
    bool     valid;
} MEM_WB_Latch;

// --- Core Structure ---
typedef struct {
    int id;

    // Hardware Components
    uint32_t regs[REG_COUNT]; // R0-R15
    uint32_t pc;
    uint32_t instruction_memory[1024]; // IMEM
    Cache l1_cache;

    // Pipeline Registers
    IF_ID_Latch  if_id;
    ID_EX_Latch  id_ex;
    EX_MEM_Latch ex_mem;
    MEM_WB_Latch mem_wb;

    // State Flags
    bool halted;       // Hit HALT instruction
    bool stall;        // Pipeline stalled (hazard or cache miss)

    // Performance Counters
    struct {
        int cycles;
        int instructions;
        int read_hits;
        int write_hits;
        int read_misses;
        int write_misses;
        int decode_stalls;
        int mem_stalls;
    } stats;
    uint32_t trace_regs[16]; // Snapshot for the trace file

    uint32_t last_wb_pc;
    bool last_wb_valid;
    bool branch_pending;
    uint32_t branch_target;
    bool halt_detected; // New flag: "We saw a HALT, stop fetching!"

} Core;

void core_init(Core *core, int id, const char *imem_path);
void core_cycle(Core *core, Bus *bus); // Runs one clock cycle

#endif // CORE_H