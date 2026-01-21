/*
 * Project: Multi-Core Cache Simulator (MIPS-like)
 * File:    cache.c
 * * Description:
 * This module implements the L1 Cache controller and the MESI (Illinois)
 * coherence protocol. It manages hit/miss detection, conflict evictions,
 * and bus snooping to maintain memory consistency across multiple cores.
 */

#include <string.h>
#include <stdio.h>
#include "cache.h"

 /**
  * @brief Resets cache storage and controller state.
  * * All cache lines are set to MESI_INVALID at startup.
  */
void cache_init(Cache* cache, int core_id) {
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

/**
 * @brief Handles core read requests.
 * * Decodes the address into Tag, Set, and Offset.
 * * If the tag matches and state is Valid (M, E, or S), it's a hit.
 * * If the tag doesn't match and the existing block is Modified, it triggers a conflict eviction.
 * * Otherwise, it initiates a BusRd transaction.
 */
bool cache_read(Cache* cache, uint32_t addr, uint32_t* data, Bus* bus) {
    uint32_t set = (addr >> 3) & 0x3F; // 6 bits for 64 sets
    uint32_t tag = addr >> 9;         // Remaining high bits
    uint32_t offset = addr & 0x7;     // 3 bits for 8 words per block

    TSRAM_Entry* entry = &cache->tsram[set];

    /* 1. HIT DETECTION */
    if (entry->state != MESI_INVALID && entry->tag == tag) {
        if (cache->pending_addr == addr) {
            // Already handled the miss for this address
            cache->pending_addr = 0xFFFFFFFF;
        }
        else {
            cache->read_hits++;
        }
        *data = cache->dsram[set][offset];
        cache->sram_check_countdown = 0;
        return true;
    }

    /* 2. CONFLICT EVICTION LOGIC */
    if (entry->state == MESI_MODIFIED && entry->tag != tag) {
        if (!cache->eviction_pending && !cache->is_flushing) {
            // Must write back the dirty line before we can reuse this set
            cache->eviction_pending = true;
            cache->flush_addr = (entry->tag << 9) | (set << 3);
            cache->flush_offset = 0;
        }
        return false; // Stall core
    }

    /* 3. MISS HANDLING */
    if (cache->pending_addr != addr) {
        // Model the tag-check latency cycle
        if (cache->sram_check_countdown == 0) {
            cache->sram_check_countdown = 1;
            return false;
        }

        cache->read_miss++;
        cache->waiting_for_write = false;
        cache->pending_addr = addr;
        cache->snoop_result_shared = false;
        cache->sram_check_countdown = 0;
    }

    return false; // Continue stalling until bus fill completes
}

/**
 * @brief Handles core write requests.
 * * Requires the block to be in Modified or Exclusive state to proceed.
 * * If the block is in Shared state or Invalid, a BusRdX (invalidation) is required.
 */
bool cache_write(Cache* cache, uint32_t addr, uint32_t data, Bus* bus) {
    uint32_t set = (addr >> 3) & 0x3F;
    uint32_t tag = addr >> 9;
    uint32_t offset = addr & 0x7;

    TSRAM_Entry* entry = &cache->tsram[set];

    // Check if we already have write permissions (M or E)
    bool write_hit = (entry->state == MESI_MODIFIED || entry->state == MESI_EXCLUSIVE) && (entry->tag == tag);

    /* 1. WRITE HIT */
    if (write_hit) {
        if (cache->pending_addr == addr) {
            cache->pending_addr = 0xFFFFFFFF;
        }
        else {
            cache->write_hits++;
        }
        cache->dsram[set][offset] = data;
        entry->state = MESI_MODIFIED; // Transition to M state
        cache->sram_check_countdown = 0;
        return true;
    }

    /* 2. CONFLICT EVICTION LOGIC */
    if (entry->state == MESI_MODIFIED && entry->tag != tag) {
        if (!cache->eviction_pending && !cache->is_flushing) {
            cache->eviction_pending = true;
            cache->flush_addr = (entry->tag << 9) | (set << 3);
            cache->flush_offset = 0;
        }
        return false;
    }

    /* 3. WRITE MISS / UPGRADE REQUEST */
    if (!cache->waiting_for_write) {
        if (cache->sram_check_countdown == 0) {
            cache->sram_check_countdown = 1;
            return false;
        }

        if (cache->pending_addr != addr) {
            cache->write_miss++;
            cache->sram_check_countdown = 0;
        }
    }

    cache->waiting_for_write = true; // Signal that we need a BusRdX (Exclusive Intent)

    if (cache->pending_addr != addr) {
        cache->pending_addr = addr;
    }

    return false;
}

/**
 * @brief Monitors the system bus and manages state transitions.
 */
void cache_snoop(Cache* cache, Bus* bus) {
    /*
     * 1. FLUSH STATE MACHINE
     * Active when this cache is providing data to the bus (eviction or snoop-hit-on-M).
     * Transmits one word per clock cycle.
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

            // Post-flush state transition
            if (cache->tsram[set].state == MESI_MODIFIED) {
                cache->tsram[set].state = MESI_INVALID;
            }
        }
        return;
    }

    /*
     * 2. SNOOPING REMOTE REQUESTS
     * Responds to BusRd and BusRdX from other cores to maintain coherence.
     */
    if (bus->bus_cmd == BUS_CMD_READ || bus->bus_cmd == BUS_CMD_READX) {
        if (bus->bus_origid == cache->core_id) return;

        uint32_t addr = bus->bus_addr;
        uint32_t set = (addr >> 3) & 0x3F;
        uint32_t tag = addr >> 9;

        TSRAM_Entry* entry = &cache->tsram[set];

        if (entry->tag == tag && entry->state != MESI_INVALID) {
            // Signal presence of data on 'bus_shared' wire
            if (bus->bus_cmd == BUS_CMD_READ && entry->state != MESI_MODIFIED) {
                bus->bus_shared = true;
            }

            // Handle snooped transitions
            if (entry->state == MESI_MODIFIED) {
                // If we have dirty data, we must provide it (intervention)
                cache->is_flushing = true;
                bus->busy = true;
                cache->flush_addr = addr & ~0x7;
                cache->flush_offset = 0;

                // Transition: M -> S (on BusRd) or M -> I (on BusRdX)
                entry->state = (bus->bus_cmd == BUS_CMD_READ) ? MESI_SHARED : MESI_INVALID;
            }
            else if (entry->state == MESI_EXCLUSIVE) {
                // E -> S (on BusRd) or E -> I (on BusRdX)
                entry->state = (bus->bus_cmd == BUS_CMD_READ) ? MESI_SHARED : MESI_INVALID;
            }
            else if (entry->state == MESI_SHARED) {
                // S -> I (on BusRdX)
                if (bus->bus_cmd == BUS_CMD_READX) {
                    entry->state = MESI_INVALID;
                }
            }
        }
    }

    /*
     * 3. DATA FILL / RESPONSE LOGIC
     * Receives data words from the bus to satisfy a local miss.
     */
    if (bus->bus_cmd == BUS_CMD_FLUSH) {
        if (bus->bus_shared) {
            // Track if other caches have this block to decide between E and S state
            cache->snoop_result_shared = true;
        }

        bool is_my_data = cache->is_waiting_for_fill &&
            ((bus->bus_addr & ~0x7) == (cache->pending_addr & ~0x7));

        if (!is_my_data) return;

        uint32_t set = (bus->bus_addr >> 3) & 0x3F;
        uint32_t tag = bus->bus_addr >> 9;
        int offset = bus->bus_addr & 0x7;

        // Store incoming word in Data SRAM
        cache->dsram[set][offset] = bus->bus_data;

        // Finalize transaction on last word (offset 7)
        if (offset == 7) {
            TSRAM_Entry* entry = &cache->tsram[set];
            entry->tag = tag;
            cache->is_waiting_for_fill = false;

            if (cache->waiting_for_write) {
                // We requested data for writing -> M state
                entry->state = MESI_MODIFIED;
                cache->waiting_for_write = false;
            }
            else {
                // We requested data for reading -> S or E state
                entry->state = (cache->snoop_result_shared) ? MESI_SHARED : MESI_EXCLUSIVE;
            }
        }
    }
}