/*
 * Project: Multi-Core Cache Simulator (MIPS-like)
 * File:    memory.c
 * * Description:
 * Implements the Main Memory controller logic. This module simulates DRAM
 * behavior, including a fixed access latency of 16 cycles for block reads.
 * It also monitors the bus for flushes to ensure memory remains consistent
 * with Modified cache lines being written back.
 */

#include <string.h>
#include <stdio.h>
#include "memory.h"

#define MEM_MASK (MAIN_MEMORY_SIZE - 1)

 /**
  * @brief Resets all memory locations to zero.
  */
void memory_init(MainMemory* mem) {
    memset(mem->data, 0, sizeof(uint32_t) * MAIN_MEMORY_SIZE);
}

/**
 * @brief Returns true if the memory controller is currently busy with a read.
 */
bool memory_is_active(MainMemory* mem) {
    return mem->processing_read;
}

/**
 * @brief Orchestrates memory-bus interactions.
 */
void memory_listen(MainMemory* mem, Bus* bus) {
    static int latency_timer = 0;
    static uint32_t target_addr = 0;
    static int word_offset = 0;

    /*
     * 1. WRITE HANDLING (FLUSH SNOOPING)
     * Memory monitors the bus for any 'Flush' command. If a core is writing back
     * a Modified block, memory captures that data to stay up-to-date.
     */
    if (bus->bus_cmd == BUS_CMD_FLUSH && bus->bus_origid < 4) {
        if (bus->bus_addr < MAIN_MEMORY_SIZE) {
            mem->data[bus->bus_addr] = bus->bus_data;
        }

        /* * Conflict Resolution: If memory was in the middle of reading a block
         * that a core just flushed, we abort our read. The core's data is newer
         * and the bus activity satisfies the original requester.
         */
        if (mem->processing_read && (bus->bus_addr & ~0x7) == (target_addr & ~0x7)) {
            mem->processing_read = false;
            latency_timer = 0;
        }
    }

    /*
     * 2. READ REQUEST INITIATION
     * When a Core issues a BusRd or BusRdX, memory starts its internal
     * counter to simulate the delay of physical DRAM sensing.
     */
    if (bus->bus_cmd == BUS_CMD_READ || bus->bus_cmd == BUS_CMD_READX) {
        if (!mem->processing_read) {
            mem->processing_read = true;
            target_addr = bus->bus_addr;
            latency_timer = 15; // Total 16 cycle delay
            word_offset = 0;
            mem->serving_shared_request = bus->bus_shared;
        }
    }

    /*
     * 3. LATENCY EMULATION & DATA DRIVE
     * Once the latency timer reaches zero, the memory controller requests
     * bus access to stream the 8-word block back to the requester.
     */
    if (mem->processing_read) {
        if (latency_timer >= 0) {
            latency_timer--;
        }
        else {
            /* * DATA TRANSFER PHASE:
             * Memory drives the bus for 8 consecutive cycles (one word per cycle).
             * Only proceeds if the Arbiter has granted the bus to Memory (ID 4).
             */
            if (bus->current_grant == 4) {
                uint32_t block_start = target_addr & ~0x7;
                uint32_t current_addr = block_start + word_offset;

                bus->bus_origid = 4;        // Driving as Memory
                bus->bus_cmd = BUS_CMD_FLUSH;
                bus->bus_addr = current_addr;
                bus->bus_data = mem->data[current_addr];

                /* * If another cache asserted 'shared' during the initial request,
                 * memory maintains that signal so the requester knows to enter
                 * the 'Shared' state instead of 'Exclusive'.
                 */
                if (mem->serving_shared_request) {
                    bus->bus_shared = true;
                }

                word_offset++;

                // End of block transfer
                if (word_offset >= 8) {
                    mem->processing_read = false;
                    bus->busy = false; // Release the bus for the next arbiter cycle
                }
            }
        }
    }
}