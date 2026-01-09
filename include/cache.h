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
    // DSRAM: Data Storage [cite: 41]
    // 64 sets, each containing a block of 8 words
    uint32_t dsram[NUM_CACHE_SETS][BLOCK_SIZE]; 
    
    // TSRAM: Tag and State Storage [cite: 42]
    TSRAM_Entry tsram[NUM_CACHE_SETS];
    
    int core_id; // To know which core owns this cache
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