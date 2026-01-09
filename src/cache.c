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
    uint32_t offset = GET_OFFSET(addr); // Helper needed for FLUSH

    // 1. Handle Coherency (Invalidate/Downgrade)
    if (cache->tsram[set].tag == tag && cache->tsram[set].state != MESI_INVALID) {
        MesiState state = cache->tsram[set].state;

        if (bus->bus_cmd == BUS_CMD_READ) {
            bus->bus_shared = 1;
            if (state == MESI_EXCLUSIVE || state == MESI_MODIFIED) {
                cache->tsram[set].state = MESI_SHARED;
                // Ideally, if Modified, we should also queue a Flush here,
                // but the spec allows the 'owner' to respond via the bus logic
                // (which we assume happens in the core/bus drive logic).
            }
        }
        else if (bus->bus_cmd == BUS_CMD_READX) {
            cache->tsram[set].state = MESI_INVALID;
        }
    }

    // 2. Handle Cache Fill (CRITICAL FIX FOR INFINITE LOOP)
    // If we see a FLUSH (Data Response), we should store it.
    // In a real CPU, we'd check if we are 'waiting' for this address (MSHR).
    // For this sim, we simply overwrite the DSRAM if it matches the bus transaction.
    // This effectively handles the "Fill" part of a Miss.
    if (bus->bus_cmd == BUS_CMD_FLUSH) {
        // Write the word into DSRAM
        cache->dsram[set][offset] = bus->bus_data;

        // The bus sends data word-by-word (Offset 0..7).
        // We only mark the line as VALID/SHARED once the LAST word arrives.
        // This ensures the core doesn't read garbage data for higher offsets
        // before they arrive.
        if (offset == 7) {
            cache->tsram[set].tag = tag;
            cache->tsram[set].state = MESI_SHARED; // Default to Shared on fill
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