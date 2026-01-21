#ifndef MEMORY_H
#define MEMORY_H

#include "global.h"
#include "bus.h"

/**
 * @brief Main Memory Structure
 * * Represents the centralized system RAM. It serves as the backing store for all
 * L1 caches and is the final destination for all 'Flush' operations.
 */
typedef struct {
    /** The physical storage array, typically sized to 2^20 words. */
    uint32_t data[MAIN_MEMORY_SIZE];

    /** Internal State: High if memory is currently timing a DRAM access latency. */
    bool processing_read;

    /** Internal State: Latches the 'bus_shared' signal from the original request. */
    bool serving_shared_request;
} MainMemory;

/**
 * @brief Initializes the Main Memory.
 * * Allocates/clears the memory array and resets controller state.
 * * @param mem Pointer to the MainMemory structure.
 */
void memory_init(MainMemory* mem);

/**
 * @brief Memory Controller Logic.
 * * Listens to the system bus and manages the timing of memory transactions.
 * * Functions:
 * 1. Snoops for BUS_CMD_FLUSH to update memory in parallel with cache-to-cache transfers.
 * 2. Snoops for BUS_CMD_READ/READX to initiate a timed data retrieval.
 * 3. Acts as a Bus Master (ID 4) to drive data back to the requesting cores.
 * * @param mem Pointer to the MainMemory structure.
 * @param bus Pointer to the shared system bus.
 */
void memory_listen(MainMemory* mem, Bus* bus);

#endif