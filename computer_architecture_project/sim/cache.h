#ifndef CACHE_H
#define CACHE_H

#include "global.h"
#include "bus.h"

/**
 * @brief TSRAM (Tag SRAM) Entry
 * * Represents a single line in the Tag storage. It tracks which memory block
 * is currently cached and its current status within the MESI coherence protocol.
 */
typedef struct {
    uint32_t tag;    /**< The high-order bits of the address identifying the cached block. */
    MesiState state; /**< Protocol state: Modified (M), Exclusive (E), Shared (S), or Invalid (I). */
} TSRAM_Entry;

/**
 * @brief Cache Structure
 * * Models a Direct-Mapped L1 Cache (64 sets) with a block size of 8 words (32 bytes).
 * This structure manages both data storage and the control logic for hits, misses,
 * and snooping.
 */
typedef struct {
    /* --- Hardware Storage --- */

    /** Data SRAM: 64 sets, 8 words per block. Total size: 2KB. */
    uint32_t dsram[NUM_CACHE_SETS][BLOCK_SIZE];

    /** Tag SRAM: 64 entries corresponding to the data sets. */
    TSRAM_Entry tsram[NUM_CACHE_SETS];

    int core_id; /**< Unique identifier of the core associated with this cache (0-3). */

    /* --- Performance Counters --- */
    int read_hits;
    int write_hits;
    int read_miss;
    int write_miss;

    /* --- Protocol & Transaction Flags --- */
    bool waiting_for_write;   /**< Set if the pending bus request was triggered by a Store (Write) operation. */
    bool snoop_result_shared; /**< Latches the 'bus_shared' signal to determine if a block should enter S or E state. */

    /* --- Internal Controller State Machine --- */
    bool is_waiting_for_fill; /**< High when the cache is stalled waiting for a block fill from the bus. */
    uint32_t pending_addr;    /**< The address of the memory access currently causing a stall. */
    bool eviction_pending;    /**< High when a 'Modified' block must be written back before a new block is loaded. */
    bool is_flushing;         /**< High when the cache is actively driving data onto the bus. */
    uint32_t flush_addr;      /**< The base address of the block being flushed to the bus. */
    int flush_offset;         /**< Counter (0-7) tracking the current word being transferred during a flush. */

    int sram_check_countdown; /**< Logic delay simulator; models the 1-cycle latency of checking the Tag SRAM. */
} Cache;

/**
 * @brief Initializes the L1 Cache.
 * * Sets all SRAM to zero, marks all blocks as 'Invalid', and resets statistics.
 * * @param cache Pointer to the Cache structure.
 * @param core_id The ID of the core owning this cache.
 */
void cache_init(Cache* cache, int core_id);

/**
 * @brief Performs a Read operation on the cache.
 * * Logic: Check for hit in TSRAM. If miss, initiate bus request and stall core.
 * * @param cache Pointer to the Cache structure.
 * @param addr The 32-bit memory address to read.
 * @param data Output pointer to store the retrieved data word.
 * @param bus Pointer to the shared system bus.
 * @return true if Read Hit (transaction complete), false if Miss (core must stall).
 */
bool cache_read(Cache* cache, uint32_t addr, uint32_t* data, Bus* bus);

/**
 * @brief Performs a Write operation on the cache.
 * * Logic: Check for Exclusive/Modified hit. If miss or Shared, initiate BusRdX and stall core.
 * * @param cache Pointer to the Cache structure.
 * @param addr The 32-bit memory address to write.
 * @param data The data word to be stored.
 * @param bus Pointer to the shared system bus.
 * @return true if Write Hit (transaction complete), false if Miss/Invalidation needed (core must stall).
 */
bool cache_write(Cache* cache, uint32_t addr, uint32_t data, Bus* bus);

/**
 * @brief Snoop Controller Logic.
 * * Monitors the bus for transactions from other cores and updates internal states.
 * * Also manages the 'Flush' state machine for providing data to others or writing back to memory.
 * * @param cache Pointer to the Cache structure.
 * @param bus Pointer to the shared system bus.
 */
void cache_snoop(Cache* cache, Bus* bus);

#endif