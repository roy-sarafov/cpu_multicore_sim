#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdint.h>
#include <stdbool.h>

/*
 * System Constants
 * Defines the architectural parameters of the simulation.
 */
#define NUM_CORES 4
#define MEM_DEPTH (1 << 20)  
                             
#define MAIN_MEMORY_SIZE (1 << 21) // 2^21 words (20-bit address space + extra bit?)

#define CACHE_SIZE 512       // Total words in cache (64 sets * 8 words)
#define BLOCK_SIZE 8         // Words per cache block
#define NUM_CACHE_SETS 64    // Number of sets in the direct-mapped cache
#define REG_COUNT 16         // Number of registers (R0-R15)


/*
 * MESI States
 * Enumeration for the cache coherence protocol states.
 */
typedef enum {
    MESI_INVALID   = 0, // Block is invalid
    MESI_SHARED    = 1, // Block is valid, clean, and may exist in other caches
    MESI_EXCLUSIVE = 2, // Block is valid, clean, and exists ONLY in this cache
    MESI_MODIFIED  = 3  // Block is valid, dirty, and exists ONLY in this cache
} MesiState;

/*
 * Opcodes
 * Enumeration of supported MIPS-like instructions.
 */
typedef enum {
    OP_ADD = 0,
    OP_SUB = 1,
    OP_AND = 2,
    OP_OR  = 3,
    OP_XOR = 4,
    OP_MUL = 5,
    OP_SLL = 6,
    OP_SRA = 7, 
    OP_SRL = 8, 
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

#endif
