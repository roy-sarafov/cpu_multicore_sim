#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief System Architectural Constants
 * * These parameters define the hardware specifications of the multi-core system.
 */
#define NUM_CORES 4          /**< Total number of processing cores in the system. */
#define MEM_DEPTH (1 << 20)  /**< Addressable depth of memory (1M entries). */

 /** Main memory size: 2^21 words (8MB total given 4 bytes per word). */
#define MAIN_MEMORY_SIZE (1 << 21) 

#define CACHE_SIZE 512       /**< Total words per L1 Cache. */
#define BLOCK_SIZE 8         /**< Cache block size in words (32 bytes). */
#define NUM_CACHE_SETS 64    /**< Direct-mapped cache: 64 sets, 8 words per set. */
#define REG_COUNT 16         /**< MIPS-like register file size (R0..R15). */


/**
 * @brief MESI Coherence Protocol States
 * * Implements the Illinois MESI protocol states to maintain consistency across caches.
 */
typedef enum {
    MESI_INVALID = 0, /**< Block is not present in the cache. */
    MESI_SHARED = 1, /**< Block is clean; exists in this and potentially other caches. */
    MESI_EXCLUSIVE = 2, /**< Block is clean; exists ONLY in this cache. */
    MESI_MODIFIED = 3  /**< Block is dirty; exists ONLY in this cache and is newer than memory. */
} MesiState;

/**
 * @brief Instruction Opcodes
 * * Supported operations for the MIPS-like RISC ISA.
 */
typedef enum {
    OP_ADD = 0,   /**< R[rd] = R[rs] + R[rt] */
    OP_SUB = 1,   /**< R[rd] = R[rs] - R[rt] */
    OP_AND = 2,   /**< R[rd] = R[rs] & R[rt] */
    OP_OR = 3,   /**< R[rd] = R[rs] | R[rt] */
    OP_XOR = 4,   /**< R[rd] = R[rs] ^ R[rt] */
    OP_MUL = 5,   /**< R[rd] = R[rs] * R[rt] */
    OP_SLL = 6,   /**< R[rd] = R[rs] << R[rt] */
    OP_SRA = 7,   /**< R[rd] = R[rs] >> R[rt] (Arithmetic) */
    OP_SRL = 8,   /**< R[rd] = R[rs] >> R[rt] (Logical) */
    OP_BEQ = 9,   /**< if(R[rs] == R[rt]) PC = R[rd] */
    OP_BNE = 10,  /**< if(R[rs] != R[rt]) PC = R[rd] */
    OP_BLT = 11,  /**< if(R[rs] < R[rt])  PC = R[rd] */
    OP_BGT = 12,  /**< if(R[rs] > R[rt])  PC = R[rd] */
    OP_BLE = 13,  /**< if(R[rs] <= R[rt]) PC = R[rd] */
    OP_BGE = 14,  /**< if(R[rs] >= R[rt]) PC = R[rd] */
    OP_JAL = 15,  /**< R[15] = PC+1; PC = R[rd] */
    OP_LW = 16,  /**< R[rd] = MEM[R[rs] + R[rt]] */
    OP_SW = 17,  /**< MEM[R[rs] + R[rt]] = R[rd] */
    OP_HALT = 20  /**< Terminate execution of the core. */
} Opcode;

#endif