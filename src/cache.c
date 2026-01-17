#include <string.h>
#include <stdio.h>
#include "cache.h"

void cache_init(Cache *cache, int core_id) {
    memset(cache->dsram, 0, sizeof(cache->dsram));
    memset(cache->tsram, 0, sizeof(cache->tsram));
    cache->core_id = core_id;
    cache->read_hits = 0;
    cache->write_hits = 0;
    cache->read_miss = 0;
    cache->write_miss = 0;
    cache->waiting_for_write = false;
    cache->snoop_result_shared = false;

    // Snoop Filter / Latency Init
    cache->is_waiting_for_fill = false;
    cache->pending_addr = 0xFFFFFFFF;
    cache->is_flushing = false;
    cache->flush_addr = 0;
    cache->flush_offset = 0;
    cache->sram_check_countdown = 0; // Initialize to 0
}

bool cache_read(Cache *cache, uint32_t addr, uint32_t *data, Bus *bus) {
    uint32_t set = (addr >> 3) & 0x3F;
    uint32_t tag = addr >> 9;
    uint32_t offset = addr & 0x7;

    TSRAM_Entry *entry = &cache->tsram[set];

    // Check for Hit
    if (entry->state != MESI_INVALID && entry->tag == tag) {
        if (cache->pending_addr == addr) {
            cache->pending_addr = 0xFFFFFFFF;
        } else {
            cache->read_hits++;
        }
        *data = cache->dsram[set][offset];

        // RESET COUNTDOWN ON SUCCESS
        cache->sram_check_countdown = 0;
        return true;
    }

    // Miss Handling
    if (!cache->is_waiting_for_fill) {

        // --- ADD THIS BLOCK ---
        // If this is the first time we see the miss, just stall.
        // Do NOT set is_waiting_for_fill yet.
        if (cache->sram_check_countdown == 0) {
            cache->sram_check_countdown = 1;
            return false; // Stall
        }
        // ----------------------

        cache->read_miss++;
        cache->waiting_for_write = false;
        cache->is_waiting_for_fill = true;
        cache->pending_addr = addr;
        cache->snoop_result_shared = false;

        // Reset countdown for the next instruction
        cache->sram_check_countdown = 0;
    }

    return false; // Stall
}

bool cache_write(Cache *cache, uint32_t addr, uint32_t data, Bus *bus) {
    uint32_t set = (addr >> 3) & 0x3F;
    uint32_t tag = addr >> 9;
    uint32_t offset = addr & 0x7;

    TSRAM_Entry *entry = &cache->tsram[set];

    bool write_hit = (entry->state == MESI_MODIFIED || entry->state == MESI_EXCLUSIVE) && (entry->tag == tag);

    if (write_hit) {
        if (cache->pending_addr == addr) {
            cache->pending_addr = 0xFFFFFFFF;
        } else {
            cache->write_hits++;
        }
        cache->dsram[set][offset] = data;
        entry->state = MESI_MODIFIED;

        // RESET COUNTDOWN ON SUCCESS
        cache->sram_check_countdown = 0;
        return true;
    }

    // Miss (or Shared upgrade needed)
    if (!cache->waiting_for_write) {

        // --- ADD THIS BLOCK ---
        if (cache->sram_check_countdown == 0) {
            cache->sram_check_countdown = 1;
            return false; // Stall core, verify tag
        }
        // ----------------------

        if (!cache->is_waiting_for_fill) {
            cache->write_miss++;
            cache->sram_check_countdown = 0; // Reset
        }
    }
    cache->waiting_for_write = true;
    if (!cache->is_waiting_for_fill) {
        cache->is_waiting_for_fill = true;
        cache->pending_addr = addr;
    }

    return false;
}

void cache_snoop(Cache *cache, Bus *bus) {
    // 1. ACTIVE FLUSH: Are we in the middle of sending data?
    if (cache->is_flushing) {

        // Lock the bus so no one else uses it while we prepare/send data
        bus->busy = true;

        // Latency State Machine
        // -1: Wait Cycle (SRAM Access)
        //  0: Start Driving Data
        if (cache->flush_offset < 0) {
            cache->flush_offset++;
            return;
        }

        // Drive the bus with our data
        bus->bus_cmd = BUS_CMD_FLUSH;
        bus->bus_addr = cache->flush_addr + cache->flush_offset;

        uint32_t set = (cache->flush_addr >> 3) & 0x3F;
        bus->bus_data = cache->dsram[set][cache->flush_offset];

        // *** THIS IS WHERE SHARED BECOMES 1 (During the Flush) ***
        bus->bus_shared = true;
        bus->bus_origid = cache->core_id;

        cache->flush_offset++;
        if (cache->flush_offset >= 8) {
            cache->is_flushing = false;
            bus->busy = false; // Unlock the bus!
        }
        return;
    }

    // 2. STANDARD SNOOPING: Listen to the bus for Requests
    if (bus->bus_cmd == BUS_CMD_READ || bus->bus_cmd == BUS_CMD_READX) {
        if (bus->bus_origid == cache->core_id) return;
        uint32_t addr = bus->bus_addr;
        uint32_t set = (addr >> 3) & 0x3F;
        uint32_t tag = addr >> 9;

        TSRAM_Entry *entry = &cache->tsram[set];

        // Check for Tag Match and Valid State
        if (entry->tag == tag && entry->state != MESI_INVALID) {

            // Rule: Only assert Shared immediately if we are NOT flushing.
            // (If we are SHARED or EXCLUSIVE, we say "I have it" now).
            if (bus->bus_cmd == BUS_CMD_READ && entry->state != MESI_MODIFIED) {
                bus->bus_shared = true;
            }

            // MODIFIED: Flush data to requester
            if (entry->state == MESI_MODIFIED) {

                // *** DELETE THE BLOCK THAT WAS HERE ***
                // We do NOT set bus_shared = true here.
                // We wait for the flush cycle (Section 1 above) to do it.

                cache->is_flushing = true;
                bus->busy = true; // Locks bus for next cycle
                cache->flush_addr = addr & ~0x7;

                // Start flush immediately next cycle (Cycle 1606)
                cache->flush_offset = 0;

                // Update state immediately
                if (bus->bus_cmd == BUS_CMD_READ) {
                    entry->state = MESI_SHARED;
                } else {
                    entry->state = MESI_INVALID;
                }
            }
            // EXCLUSIVE:
            else if (entry->state == MESI_EXCLUSIVE) {
                if (bus->bus_cmd == BUS_CMD_READ) {
                    entry->state = MESI_SHARED;
                } else {
                    entry->state = MESI_INVALID;
                }
            }
            // SHARED:
            else if (entry->state == MESI_SHARED) {
                if (bus->bus_cmd == BUS_CMD_READX) {
                    entry->state = MESI_INVALID;
                }
            }
        }
    }

    // 3. CACHE FILL: Accepting Data from Bus
    if (bus->bus_cmd == BUS_CMD_FLUSH) {

        // *** CATCH THE SHARED SIGNAL HERE ***
        // Since we suppressed it during the request, the requester learns it here.
        if (bus->bus_shared) {
            cache->snoop_result_shared = true;
        }

        // SNOOP FILTER
        bool is_my_data = cache->is_waiting_for_fill &&
                          ((bus->bus_addr & ~0x7) == (cache->pending_addr & ~0x7));

        if (!is_my_data) return;

        uint32_t set = (bus->bus_addr >> 3) & 0x3F;
        uint32_t tag = bus->bus_addr >> 9;
        int offset = bus->bus_addr & 0x7;

        cache->dsram[set][offset] = bus->bus_data;

        if (offset == 7) {
            TSRAM_Entry *entry = &cache->tsram[set];
            entry->tag = tag;
            cache->is_waiting_for_fill = false;

            if (cache->waiting_for_write) {
                entry->state = MESI_MODIFIED;
                cache->waiting_for_write = false;
            } else {
                if (cache->snoop_result_shared) {
                    entry->state = MESI_SHARED;
                } else {
                    entry->state = MESI_EXCLUSIVE;
                }
            }
        }
    }
}