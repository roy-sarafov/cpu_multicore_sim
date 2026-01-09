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

    // Check if we have this block
    if (cache->tsram[set].tag != tag || cache->tsram[set].state == MESI_INVALID) {
        return; // Miss in snoop, nothing to do
    }

    // Snoop Hit!
    MesiState state = cache->tsram[set].state;

    // "For BusRd... bus_shared set to 1 if any core has data" [cite: 58]
    if (bus->bus_cmd == BUS_CMD_READ) {
        bus->bus_shared = 1;

        if (state == MESI_EXCLUSIVE || state == MESI_MODIFIED) {
            // Downgrade to Shared
            cache->tsram[set].state = MESI_SHARED;

            // If Modified, we must FLUSH data to memory/bus [cite: 59]
            // Note: The actual flush logic requires grabbing the bus,
            // which is complex in a cycle-step function.
            // Usually, simulators treat this as an immediate intervention or
            // queue a Flush request for the next arbitration.
        }
    }

    // "BusRdX... invalidates other copies" [cite: 53]
    else if (bus->bus_cmd == BUS_CMD_READX) {
        if (state == MESI_MODIFIED) {
            // If we have modified data, we must flush it before invalidating
             // (Logic handled by core/bus controller usually)
        }
        cache->tsram[set].state = MESI_INVALID;
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
    cache->write_miss++;
    // Core must stall and issue BusRdX (Write Allocate)
    return false;
}