#ifndef CACHE_H
#define CACHE_H

#include "global.h"
#include "bus.h"

/*
 * TSRAM Entry
 * Represents a single entry in the Tag Static RAM.
 */
typedef struct {
    uint32_t tag;    // The tag bits of the stored address
    MesiState state; // The MESI state of the block (Modified, Exclusive, Shared, Invalid)
} TSRAM_Entry;

/*
 * Cache Structure
 * Represents the L1 Cache hardware for a single core.
 */
typedef struct {
    // --- Storage ---
    // Data SRAM: 64 sets, 8 words (32 bytes) per block
    uint32_t dsram[NUM_CACHE_SETS][BLOCK_SIZE];

    // Tag SRAM: 64 entries
    TSRAM_Entry tsram[NUM_CACHE_SETS];

    int core_id; // ID of the core owning this cache

    // --- Statistics ---
    int read_hits;
    int write_hits;
    int read_miss;
    int write_miss;

    // --- Internal State Flags ---
    bool waiting_for_write;   // True if the pending operation is a Write (Store)
    bool snoop_result_shared; // True if another cache asserted Shared during our request

    // --- State Machine Flags ---
    bool is_waiting_for_fill; // True if we are waiting for data from Bus/Memory
    uint32_t pending_addr;    // The address we are currently trying to access

    bool is_flushing;         // True if we are currently flushing a block to the bus
    uint32_t flush_addr;      // The address of the block being flushed
    int flush_offset;         // Current word index (0-7) being flushed
    
    int sram_check_countdown; // Timer to simulate SRAM access latency (1 cycle)
} Cache;

/*
 * cache_init
 * Initializes the cache structure, clearing memory and resetting stats.
 */
void cache_init(Cache *cache, int core_id);

/*
 * cache_read
 * Attempts to read a word from the cache.
 * Returns true if Hit, false if Miss (stall required).
 */
bool cache_read(Cache *cache, uint32_t addr, uint32_t *data, Bus *bus);

/*
 * cache_write
 * Attempts to write a word to the cache.
 * Returns true if Hit (and write completed), false if Miss (stall required).
 */
bool cache_write(Cache *cache, uint32_t addr, uint32_t data, Bus *bus);

/*
 * cache_snoop
 * Listens to the bus for transactions from other cores.
 * Updates MESI state, asserts Shared signal, and triggers flushes if necessary.
 */
void cache_snoop(Cache *cache, Bus *bus);

#endif
