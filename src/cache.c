#include <string.h>
#include "cache.h"

// Helper: Extract Tag and Set Index from Address
// Address: [Tag (19 bits) | Set Index (6 bits) | Offset (3 bits) | 2 bits byte offset ignored]
// Spec: Cache 512 words, Block 8 words -> 64 sets.
// 32-bit addr >> 2 (word aligned).
// Lower 3 bits: Block Offset. Next 6 bits: Set Index. Rest: Tag.
#define GET_OFFSET(addr) (addr & 0x7)
#define GET_SET(addr)    ((addr >> 3) & 0x3F)
#define GET_TAG(addr)    ((addr >> 9))

void cache_init(Cache *cache, int core_id) {
    memset(cache, 0, sizeof(Cache));
    cache->core_id = core_id;
}

// -------------------------------------------------------------------------
// Snooping Logic (Called every cycle to watch the bus)
// -------------------------------------------------------------------------
void cache_snoop(Cache *cache, Bus *bus) {
    // If this core is the one originating the command, ignore snoop
    if (bus->bus_origid == cache->core_id) return;
    if (bus->bus_cmd == BUS_CMD_NO_CMD) return;

    uint32_t addr = bus->bus_addr;
    uint32_t set = GET_SET(addr);
    uint32_t tag = GET_TAG(addr);
    uint32_t offset = GET_OFFSET(addr);

    // 1. Handle Coherency (Invalidate/Downgrade)
    if (cache->tsram[set].tag == tag && cache->tsram[set].state != MESI_INVALID) {
        MesiState state = cache->tsram[set].state;

        if (bus->bus_cmd == BUS_CMD_READ) {
            bus->bus_shared = 1;
            if (state == MESI_EXCLUSIVE || state == MESI_MODIFIED) {
                cache->tsram[set].state = MESI_SHARED;
            }
        }
        else if (bus->bus_cmd == BUS_CMD_READX) {
            cache->tsram[set].state = MESI_INVALID;
        }
    }

    // 2. Handle Cache Fill
    if (bus->bus_cmd == BUS_CMD_FLUSH) {
        cache->dsram[set][offset] = bus->bus_data;

        if (offset == 7) {
            cache->tsram[set].tag = tag;

            if (cache->waiting_for_write) {
                // Was waiting for BusRdX (Write Miss) -> Always Modified
                cache->tsram[set].state = MESI_MODIFIED;
                cache->waiting_for_write = false;
            } else {
                // Was waiting for BusRd (Read Miss) -> Check the latched signal!

                // --- FIX START ---
                if (cache->snoop_result_shared) {
                    cache->tsram[set].state = MESI_SHARED;     // Someone else has it
                } else {
                    cache->tsram[set].state = MESI_EXCLUSIVE;  // MINE ALONE!
                }
                // --- FIX END ---
            }
        }
    }
}
// -------------------------------------------------------------------------
// Core Read/Write Interface
// -------------------------------------------------------------------------

bool cache_read(Cache *cache, uint32_t addr, uint32_t *data, Bus *bus) {
    uint32_t set = GET_SET(addr);
    uint32_t tag = GET_TAG(addr);
    uint32_t offset = GET_OFFSET(addr);

    // Check Tag and Valid Bit
    if (cache->tsram[set].tag == tag && cache->tsram[set].state != MESI_INVALID) {
        // HIT
        *data = cache->dsram[set][offset];
        cache->read_hits++;
        return true;
    }

    // MISS
    cache->read_miss++;

    // --- [CRITICAL FIX] ---
    // We must signal that we are waiting for a READ operation.
    // This ensures cache_snoop sets the state to MESI_SHARED when data arrives.
    cache->waiting_for_write = false;

    // Note: The logic to FILL the cache happens when the data returns from the Bus.
    // The Core sees 'false' here, Stalls, and requests the Bus.
    return false;
}

bool cache_write(Cache *cache, uint32_t addr, uint32_t data, Bus *bus) {
    uint32_t set = GET_SET(addr);
    uint32_t tag = GET_TAG(addr);
    uint32_t offset = GET_OFFSET(addr);

    // Check Tag and Valid
    if (cache->tsram[set].tag == tag && cache->tsram[set].state != MESI_INVALID) {
        // HIT

        // If state is Shared, we need to upgrade to Exclusive/Modified (BusRdX)
        if (cache->tsram[set].state == MESI_SHARED) {
            // --- [ADDITION 1] ---
            // We are stalling to request ownership. Mark this!
            cache->waiting_for_write = true;

            // Return false to tell Core to stall and issue BusRdX
            return false;
        }

        // Write Hit (Exclusive or Modified)
        cache->dsram[set][offset] = data;
        cache->tsram[set].state = MESI_MODIFIED;
        cache->write_hits++;
        return true;
    }

    // MISS
    // --- [MODIFICATION 2] ---
    // Prevent stat explosion: Only increment miss count if we weren't ALREADY waiting
    if (!cache->waiting_for_write) {
        cache->write_miss++;
    }

    // --- [ADDITION 3] ---
    // We are stalling for a Write Miss (Write Allocate). Mark this!
    cache->waiting_for_write = true;

    // Core must stall and issue BusRdX (Write Allocate)
    return false;
}