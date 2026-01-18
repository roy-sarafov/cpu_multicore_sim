#ifndef CORE_H
#define CORE_H

#include "global.h"
#include "cache.h"
#include "bus.h"

/*
 * Pipeline Latches
 * These structures hold the data passed between pipeline stages.
 */

// Fetch -> Decode
typedef struct {
    uint32_t PC;          // PC of the instruction
    uint32_t Instruction; // The raw 32-bit instruction
} IF_ID_Latch;

// Decode -> Execute
typedef struct {
    uint32_t PC;
    uint32_t A;           // Value of Rs
    uint32_t B;           // Value of Rt
    uint32_t Imm;         // Sign-extended immediate
    uint32_t Rd_Index;    // Destination register index
    uint32_t Rt_Index;    // Source/Dest register index (depending on op)
    uint32_t Rs_Index;    // Source register index
    Opcode   Op;          // Decoded Opcode
    bool     valid;       // True if latch contains a valid instruction (not a bubble)
} ID_EX_Latch;

// Execute -> Memory
typedef struct {
    uint32_t PC;
    uint32_t ALUOutput;   // Result of ALU calculation (or effective address)
    uint32_t B;           // Value of Rt (for Store operations)
    uint32_t Rd_Index;    // Destination register index
    Opcode   Op;
    bool     valid;
} EX_MEM_Latch;

// Memory -> WriteBack
typedef struct {
    uint32_t PC;
    uint32_t MemData;     // Data read from memory (for Load operations)
    uint32_t ALUOutput;   // ALU result passed through
    uint32_t Rd_Index;    // Destination register index
    Opcode   Op;
    bool     valid;
} MEM_WB_Latch;

/*
 * Core Structure
 * Represents the state of a single MIPS core.
 */
typedef struct {
    int id; // Core ID (0-3)

    // --- Architectural State ---
    uint32_t regs[REG_COUNT];          // Register File (R0-R15)
    uint32_t pc;                       // Program Counter
    uint32_t instruction_memory[1024]; // Instruction Memory (IMEM)
    Cache l1_cache;                    // L1 Cache Instance

    // --- Pipeline Registers ---
    IF_ID_Latch  if_id;
    ID_EX_Latch  id_ex;
    EX_MEM_Latch ex_mem;
    MEM_WB_Latch mem_wb;

    // --- Control Flags ---
    bool halted;       // True if the core has executed a HALT instruction
    bool stall;        // True if the pipeline is stalled (e.g., waiting for memory)

    // --- Statistics ---
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

    uint32_t trace_regs[16]; // Copy of regs for trace generation (snapshot)

    // --- Internal Logic Flags ---
    bool branch_pending;    // True if a branch was taken in Decode
    uint32_t branch_target; // Target address of the branch
    bool halt_detected;     // True if HALT was seen in Decode (stops Fetch)
    int wb_hazard_rd;       // Register being written in WB stage (for hazard detection)

} Core;

/*
 * core_init
 * Initializes the core, loads IMEM from file, and resets state.
 */
void core_init(Core *core, int id, const char *imem_path);

/*
 * core_cycle
 * Advances the core by one clock cycle.
 * Executes all pipeline stages in reverse order (WB -> MEM -> EX -> ID -> IF).
 */
void core_cycle(Core *core, Bus *bus);

#endif
