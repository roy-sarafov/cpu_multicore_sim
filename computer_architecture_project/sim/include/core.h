#ifndef CORE_H
#define CORE_H

#include "global.h"
#include "cache.h"
#include "bus.h"

/**
 * @brief Pipeline Inter-Stage Latches
 * * These structures represent the registers that separate the 5 stages of the MIPS pipeline.
 * Data is sampled into these latches at the end of each clock cycle.
 */

 /** Fetch -> Decode Latch (IF/ID) */
typedef struct {
    uint32_t PC;          /**< The address of the instruction currently being decoded. */
    uint32_t Instruction; /**< The raw 32-bit machine code fetched from memory. */
} IF_ID_Latch;

/** Decode -> Execute Latch (ID/EX) */
typedef struct {
    uint32_t PC;
    uint32_t A;           /**< The value retrieved from register Rs (or immediate). */
    uint32_t B;           /**< The value retrieved from register Rt (or immediate). */
    uint32_t Imm;         /**< The 32-bit sign-extended immediate value. */
    uint32_t Rd_Index;    /**< The destination register index (Rd). */
    uint32_t Rt_Index;    /**< The source/destination register index (Rt). */
    uint32_t Rs_Index;    /**< The primary source register index (Rs). */
    Opcode   Op;          /**< The decoded operation to be performed by the ALU. */
    bool     valid;       /**< Control flag: False indicates a pipeline bubble (NOP). */
} ID_EX_Latch;

/** Execute -> Memory Latch (EX/MEM) */
typedef struct {
    uint32_t PC;
    uint32_t ALUOutput;   /**< The result of the ALU operation or the calculated effective address. */
    uint32_t B;           /**< The data word to be stored (for SW instructions). */
    uint32_t Rd_Index;    /**< The destination register index. */
    Opcode   Op;
    bool     valid;
} EX_MEM_Latch;

/** Memory -> WriteBack Latch (MEM/WB) */
typedef struct {
    uint32_t PC;
    uint32_t MemData;     /**< The data word loaded from the cache (for LW instructions). */
    uint32_t ALUOutput;   /**< The ALU result passed through for register writing. */
    uint32_t Rd_Index;    /**< The destination register index. */
    Opcode   Op;
    bool     valid;
} MEM_WB_Latch;

/**
 * @brief Core State Structure
 * * Represents the complete architectural and microarchitectural state of a single processor core.
 */
typedef struct {
    int id; /**< Core ID (0 to 3). */

    /* --- Architectural State --- */
    uint32_t regs[REG_COUNT];          /**< General Purpose Registers (R0-R15). R0=0, R1=Imm. */
    uint32_t pc;                       /**< Program Counter (instruction pointer). */
    uint32_t instruction_memory[1024]; /**< Local Instruction Memory (IMEM). */
    Cache l1_cache;                    /**< Local L1 Cache controller and storage. */

    /* --- Pipeline Registers --- */
    IF_ID_Latch  if_id;
    ID_EX_Latch  id_ex;
    EX_MEM_Latch ex_mem;
    MEM_WB_Latch mem_wb;

    /* --- Control & Pipeline Management --- */
    bool halted;       /**< Asserted when a HALT instruction reaches the WB stage. */
    bool stall;        /**< Global stall signal (e.g., triggered by cache misses). */

    /* --- Performance Statistics --- */
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

    uint32_t trace_regs[16]; /**< Register snapshot used for generating cycle-by-cycle traces. */

    /* --- Microarchitectural Helper Flags --- */
    bool branch_pending;    /**< True if a branch target has been resolved in Decode. */
    uint32_t branch_target; /**< The target address to load into the PC next cycle. */
    bool halt_detected;     /**< Stops Fetch as soon as HALT is decoded to prevent over-fetching. */
    int wb_hazard_rd;       /**< Tracks the register being written in the current WB cycle for hazard checking. */

} Core;

/**
 * @brief Initializes the core.
 * * Sets up the cache, resets registers, and loads instructions from the IMEM file.
 */
void core_init(Core* core, int id, const char* imem_path);

/**
 * @brief Performs one clock cycle of the core.
 * * Processes the pipeline stages in reverse (WB to IF) to properly simulate latch transitions.
 */
void core_cycle(Core* core, Bus* bus);

#endif