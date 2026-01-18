#ifndef MEMORY_H
#define MEMORY_H

#include "global.h"
#include "bus.h"

/*
 * MainMemory Structure
 * Represents the main system memory (DRAM).
 */
typedef struct {
    uint32_t data[MAIN_MEMORY_SIZE]; // The actual memory storage array
    bool processing_read;            // True if memory is currently handling a read request (latency)
    bool serving_shared_request;     // True if the current read request was flagged as Shared
} MainMemory;

/*
 * memory_init
 * Initializes main memory, clearing all data to zero.
 */
void memory_init(MainMemory *mem);

/*
 * memory_listen
 * The main logic function for memory.
 * - Listens to the bus for Read/Write commands.
 * - Handles write-backs (Flush) immediately.
 * - Simulates latency for Read requests.
 * - Drives the bus to return data after latency expires.
 */
void memory_listen(MainMemory *mem, Bus *bus);

#endif
