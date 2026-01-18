#include <string.h>
#include <stdio.h>
#include "memory.h"

#define MEM_MASK (MAIN_MEMORY_SIZE - 1)

void memory_init(MainMemory *mem) {
    memset(mem->data, 0, sizeof(uint32_t) * MAIN_MEMORY_SIZE);
}

bool memory_is_active(MainMemory *mem) {
    return mem->processing_read;
}

void memory_listen(MainMemory *mem, Bus *bus) {
    static int latency_timer = 0;
    static uint32_t target_addr = 0;
    static int word_offset = 0;

    if (bus->bus_cmd == BUS_CMD_FLUSH && bus->bus_origid < 4) {
        if (bus->bus_addr < MAIN_MEMORY_SIZE) {
            mem->data[bus->bus_addr] = bus->bus_data;
        }

        if (mem->processing_read && (bus->bus_addr & ~0x7) == (target_addr & ~0x7)) {
            mem->processing_read = false;
            latency_timer = 0;
        }
    }

    if (bus->bus_cmd == BUS_CMD_READ || bus->bus_cmd == BUS_CMD_READX) {
        if (!mem->processing_read) {
            mem->processing_read = true;
            target_addr = bus->bus_addr;
            latency_timer = 15;
            word_offset = 0;
            mem->serving_shared_request = bus->bus_shared;
        }
    }

    if (mem->processing_read) {
        if (latency_timer >= 0) {
            latency_timer--;
        } else {
            if (bus->current_grant == 4) {
                uint32_t block_start = target_addr & ~0x7;
                uint32_t current_addr = block_start + word_offset;

                bus->bus_origid = 4;
                bus->bus_cmd = BUS_CMD_FLUSH;
                bus->bus_addr = current_addr;
                bus->bus_data = mem->data[current_addr];
                if (mem->serving_shared_request) {
                    bus->bus_shared = true;
                }

                word_offset++;

                if (word_offset >= 8) {
                    mem->processing_read = false;
                    bus->busy = false;
                }
            }
        }
    }
}
