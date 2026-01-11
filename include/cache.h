#ifndef CACHE_H
#define CACHE_H

#include "global.h"
#include "bus.h"

// Structure for TSRAM entry (Tag + MESI)
typedef struct {
    uint32_t tag;   // Bits 11:0 of the entry [cite: 43]
    MesiState state; // Bits 13:12 of the entry [cite: 43]
} TSRAM_Entry;

typedef struct {
    // DSRAM: Data Storage
    // 64 sets, each containing a block of 8 words
    uint32_t dsram[NUM_CACHE_SETS][BLOCK_SIZE];

    // TSRAM: Tag and State Storage
    TSRAM_Entry tsram[NUM_CACHE_SETS];

    int core_id; // To know which core owns this cache

    // Statistics
    int read_hits;
    int write_hits;
    int read_miss;
    int write_miss;

    // Existing Flags
    bool waiting_for_write;   // True if the pending fill is for a Store (BusRdX)
    bool snoop_result_shared; // Stores if the bus line was shared during our request

    // --- NEW FIELDS FOR SNOOP FILTERING ---
    bool is_waiting_for_fill; // <--- ADD THIS: Are we currently waiting for data from memory?
    uint32_t pending_addr;    // <--- ADD THIS: The exact address we are waiting for.
    bool is_flushing;       // Are we currently sending data to the bus?
    uint32_t flush_addr;    // The address we are flushing
    int flush_offset;       // Current word offset (0-7) being flushed
} Cache;

void cache_init(Cache *cache, int core_id);

// Core Interface
// Returns true on Hit, False on Miss (and initiates Bus transaction)
bool cache_read(Cache *cache, uint32_t addr, uint32_t *data, Bus *bus);
bool cache_write(Cache *cache, uint32_t addr, uint32_t data, Bus *bus);

// Bus Snooping Interface
// Listening to other transactions on the bus to update MESI state
void cache_snoop(Cache *cache, Bus *bus);

#endif // CACHE_H