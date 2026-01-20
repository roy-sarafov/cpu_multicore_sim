/*
 * Project: Multi-Core Cache Simulator (MIPS-like)
 * File:    cache.c
 * Author:
 * ID:
 * Date:    11/11/2024
 *
 * Description:
 * Implements the L1 Cache logic, including MESI protocol state transitions,
 * hit/miss detection, and snooping. Handles data storage (DSRAM) and tag
 * storage (TSRAM) updates.
 */

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
    cache->is_waiting_for_fill = false;
    cache->pending_addr = 0xFFFFFFFF;
    cache->is_flushing = false;
    cache->flush_addr = 0;
    cache->flush_offset = 0;
    cache->sram_check_countdown = 0;
    cache->eviction_pending = false;
}

bool cache_read(Cache *cache, uint32_t addr, uint32_t *data, Bus *bus) {
    uint32_t set = (addr >> 3) & 0x3F;
    uint32_t tag = addr >> 9;
    uint32_t offset = addr & 0x7;

    TSRAM_Entry *entry = &cache->tsram[set];

    /*
     * 1. HIT DETECTION
     * If the tag matches and the line is valid, we have a hit.
     * We return the data immediately.
     */
    if (entry->state != MESI_INVALID && entry->tag == tag) {
        if (cache->pending_addr == addr) {
            cache->pending_addr = 0xFFFFFFFF;
        } else {
            cache->read_hits++;
        }
        *data = cache->dsram[set][offset];
        cache->sram_check_countdown = 0;
        return true;
    }

    /*
 * 2. CONFLICT EVICTION LOGIC
 * If we missed, and the CURRENT line is Modified, we must flush it first.
 * We request the bus for this eviction (eviction_pending) and stall.
 */
    if (entry->state == MESI_MODIFIED && entry->tag != tag) {
        if (!cache->eviction_pending && !cache->is_flushing) {
            cache->eviction_pending = true; // Request arbitration
            cache->flush_addr = (entry->tag << 9) | (set << 3);
            cache->flush_offset = 0;
        }
        return false; // Stall core until eviction is done
    }

    /*
     * 3. MISS HANDLING
     * If we are not already waiting for this address, we register a miss.
     * We also enforce a 1-cycle stall to simulate tag check latency.
     */
    if (cache->pending_addr != addr) {

        if (cache->sram_check_countdown == 0) {
            cache->sram_check_countdown = 1;
            return false; // Stall
        }

        cache->read_miss++;
        cache->waiting_for_write = false;

        cache->pending_addr = addr;

        cache->snoop_result_shared = false;
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

    /*
     * 1. WRITE HIT
     * If we have the block in Modified or Exclusive state, we can write directly.
     * The state becomes Modified.
     */
    if (write_hit) {
        if (cache->pending_addr == addr) {
            cache->pending_addr = 0xFFFFFFFF;
        } else {
            cache->write_hits++;
        }
        cache->dsram[set][offset] = data;
        entry->state = MESI_MODIFIED;
        cache->sram_check_countdown = 0;
        return true;
    }

    /*
  * 2. CONFLICT EVICTION LOGIC
  * If we missed, and the CURRENT line is Modified, we must flush it first.
  * We request the bus for this eviction (eviction_pending) and stall.
  */
    if (entry->state == MESI_MODIFIED && entry->tag != tag) {
        if (!cache->eviction_pending && !cache->is_flushing) {
            cache->eviction_pending = true; // Request arbitration
            cache->flush_addr = (entry->tag << 9) | (set << 3);
            cache->flush_offset = 0;
        }
        return false; // Stall core until eviction is done
    }

    /*
     * 3. WRITE MISS / UPGRADE
     * If we don't have the block (or it's Shared), we need to request it (ReadX).
     */
    if (!cache->waiting_for_write) {
        if (cache->sram_check_countdown == 0) {
            cache->sram_check_countdown = 1;
            return false; // Stall core, verify tag
        }

        if (cache->pending_addr != addr) {
            cache->write_miss++;
            cache->sram_check_countdown = 0;
        }
    }

    cache->waiting_for_write = true;

    if (cache->pending_addr != addr) {
        cache->pending_addr = addr;
    }

    return false;
}

void cache_snoop(Cache *cache, Bus *bus) {
    /*
     * 1. FLUSH STATE MACHINE
     * If this cache is flushing data (due to eviction or snoop),
     * it drives the bus with the data words one by one.
     */
    if (cache->is_flushing) {
        bus->busy = true;
        if (cache->flush_offset < 0) {
            cache->flush_offset++;
            return;
        }
        bus->bus_cmd = BUS_CMD_FLUSH;
        bus->bus_addr = cache->flush_addr + cache->flush_offset;

        uint32_t set = (cache->flush_addr >> 3) & 0x3F;
        bus->bus_data = cache->dsram[set][cache->flush_offset];

        bus->bus_shared = true;
        bus->bus_origid = cache->core_id;

        cache->flush_offset++;
        if (cache->flush_offset >= 8) {
            cache->is_flushing = false;
            bus->busy = false;

            // If we were flushing due to eviction, invalidate the line.
            // If flushing due to Snoop (Modified -> Shared/Invalid), state is handled below.
            if (cache->tsram[set].state == MESI_MODIFIED) {
                cache->tsram[set].state = MESI_INVALID;
            }
        }
        return;
    }

    /*
     * 2. SNOOPING REQUESTS
     * Listen for Read or ReadX commands from other cores.
     * Update MESI state and trigger flushes if necessary.
     */
    if (bus->bus_cmd == BUS_CMD_READ || bus->bus_cmd == BUS_CMD_READX) {
        if (bus->bus_origid == cache->core_id) return;
        uint32_t addr = bus->bus_addr;
        uint32_t set = (addr >> 3) & 0x3F;
        uint32_t tag = addr >> 9;

        TSRAM_Entry *entry = &cache->tsram[set];

        if (entry->tag == tag && entry->state != MESI_INVALID) {
            // Assert Shared signal if we have the copy
            if (bus->bus_cmd == BUS_CMD_READ && entry->state != MESI_MODIFIED) {
                bus->bus_shared = true;
            }
            // If Modified, we must flush to memory/requester
            if (entry->state == MESI_MODIFIED) {
                cache->is_flushing = true;
                bus->busy = true;
                cache->flush_addr = addr & ~0x7;
                cache->flush_offset = 0;
                if (bus->bus_cmd == BUS_CMD_READ) {
                    entry->state = MESI_SHARED;
                } else {
                    entry->state = MESI_INVALID;
                }
            }
            else if (entry->state == MESI_EXCLUSIVE) {
                if (bus->bus_cmd == BUS_CMD_READ) {
                    entry->state = MESI_SHARED;
                } else {
                    entry->state = MESI_INVALID;
                }
            }
            else if (entry->state == MESI_SHARED) {
                if (bus->bus_cmd == BUS_CMD_READX) {
                    entry->state = MESI_INVALID;
                }
            }
        }
    }

    /*
     * 3. DATA FILL
     * Accept data from the bus (Flush command) if we are waiting for it.
     */
    if (bus->bus_cmd == BUS_CMD_FLUSH) {
        if (bus->bus_shared) {
            cache->snoop_result_shared = true;
        }

        bool is_my_data = cache->is_waiting_for_fill &&
                          ((bus->bus_addr & ~0x7) == (cache->pending_addr & ~0x7));

        if (!is_my_data) return;

        uint32_t set = (bus->bus_addr >> 3) & 0x3F;
        uint32_t tag = bus->bus_addr >> 9;
        int offset = bus->bus_addr & 0x7;

        cache->dsram[set][offset] = bus->bus_data;

        // When the last word arrives, update state
        if (offset == 7) {
            TSRAM_Entry *entry = &cache->tsram[set];
            entry->tag = tag;
            cache->is_waiting_for_fill = false; // Transaction done

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
