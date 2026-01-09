#include <string.h>
#include <stdio.h> // For error logging
#include "memory.h"

// Helper to extract the 21-bit address from the 32-bit word
// (Though spec says address space is 21 bits, we usually just mask if needed)
#define MEM_MASK (MAIN_MEMORY_SIZE - 1)

void memory_init(MainMemory *mem) {
    memset(mem->data, 0, sizeof(uint32_t) * MAIN_MEMORY_SIZE);
}

void memory_listen(MainMemory *mem, Bus *bus) {
    static int latency_timer = 0;
    static uint32_t target_addr = 0;
    static bool processing_read = false;
    static int word_offset = 0; // Sending words 0..7 of the block

    // 1. Check for incoming writes (Flush from a Core)
    // "If data is Modified in another core... that core returns the block...
    // and main memory updates in parallel" [cite: 59]
    if (bus->bus_cmd == BUS_CMD_FLUSH && bus->bus_origid < 4) {
        // Write the data to main memory
        if (bus->bus_addr < MAIN_MEMORY_SIZE) {
            mem->data[bus->bus_addr] = bus->bus_data;
        }

        // If memory was trying to respond to a read for this address,
        // the Core's Flush overrides us (or satisfies the request).
        // We abort our own read processing to avoid bus contention.
        if (processing_read && (bus->bus_addr & ~0x7) == (target_addr & ~0x7)) {
            processing_read = false;
            latency_timer = 0;
        }
    }

    // 2. Handle Read Requests
    if (bus->bus_cmd == BUS_CMD_READ || bus->bus_cmd == BUS_CMD_READX) {
        // Only accept if we aren't already busy, or if it's a new request
        if (!processing_read) {
            processing_read = true;
            target_addr = bus->bus_addr;
            latency_timer = 16; // "First word... latency of 16 clock cycles" [cite: 55]
            word_offset = 0;
        }
    }

    // 3. Process Latency and Send Data
    if (processing_read) {
        if (latency_timer > 0) {
            latency_timer--;
        } else {
            // Timer finished, we need the bus to send data
            // Note: In a real simulation, Memory needs to request bus access via arbitration.
            // For simplicity here, we assume if `bus->current_grant == 4`, we send.

            // We are sending a block of 8 words
            if (bus->current_grant == 4) {
                // Calculate address of current word in the block
                // Block alignment: lower 3 bits are 0
                uint32_t block_start = target_addr & ~0x7;
                uint32_t current_addr = block_start + word_offset;

                bus->bus_origid = 4; // Memory ID
                bus->bus_cmd = BUS_CMD_FLUSH;
                bus->bus_addr = current_addr;
                bus->bus_data = mem->data[current_addr];

                word_offset++;

                // If we sent the last word (offset 8), we are done
                if (word_offset >= 8) {
                    processing_read = false;
                    bus->busy = false; // Release bus
                }
            }
        }
    }
}
