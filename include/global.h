#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdint.h>
#include <stdbool.h>

// --- System Constants ---
#define NUM_CORES 4
#define MEM_DEPTH (1 << 20)  // 2^20 words (Note: PDF says 2^21 address space, but check if word-addressable)
                             // Correction: PDF says "Address space is 21 bits... Memory size is 2^21 words" [cite: 47]
#define MAIN_MEMORY_SIZE (1 << 21) 

#define CACHE_SIZE 512       // 512 Words DSRAM [cite: 41]
#define BLOCK_SIZE 8         // 8 Words per block [cite: 40]
#define NUM_CACHE_SETS 64    // 512 / 8 = 64 sets [cite: 42]
#define REG_COUNT 16         // Registers R0-R15 [cite: 20]

// --- Protocol Definitions ---
// MESI States for TSRAM (bits 13:12) [cite: 43]
typedef enum {
    MESI_INVALID   = 0,
    MESI_SHARED    = 1,
    MESI_EXCLUSIVE = 2,
    MESI_MODIFIED  = 3
} MesiState;

// Opcode Definitions [cite: 64]
typedef enum {
    OP_ADD = 0,
    OP_SUB = 1,
    OP_AND = 2,
    OP_OR  = 3,
    OP_XOR = 4,
    OP_MUL = 5,
    OP_SLL = 6,
    OP_SRA = 7, // Arithmetic shift with sign extension
    OP_SRL = 8, // Logical shift
    OP_BEQ = 9,
    OP_BNE = 10,
    OP_BLT = 11,
    OP_BGT = 12,
    OP_BLE = 13,
    OP_BGE = 14,
    OP_JAL = 15,
    OP_LW  = 16,
    OP_SW  = 17,
    OP_HALT = 20
} Opcode;

#endif // GLOBAL_H