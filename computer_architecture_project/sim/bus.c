/*
 * Project: Multi-Core Cache Simulator (MIPS-like)
 * File:    bus.c
 * * Description:
 * This module implements the shared system bus logic. It facilitates communication
 * between the multiple CPU cores and Main Memory. The bus handles command
 * broadcasting (snooping) and uses a Round-Robin arbitration scheme to ensure
 * fair access among all system components.
 */

#include <string.h>
#include "bus.h"

 /**
  * @brief Initializes the bus state.
  * * Clears the entire structure and sets the initial Round-Robin pointer.
  * The index is initialized to 4 (Memory) so that the first arbitration
  * cycle begins checking Core 0.
  */
void bus_init(Bus* bus) {
    memset(bus, 0, sizeof(Bus));
    bus->arbitration_rr_index = 4;
}

/**
 * @brief Resets the bus wires/signals for the current cycle.
 * * This simulates the bus signals returning to a neutral state between
 * transactions. Internal state such as the current master and arbiter
 * history are preserved.
 */
void bus_reset_signals(Bus* bus) {
    bus->bus_origid = 0;
    bus->bus_cmd = BUS_CMD_NO_CMD;
    bus->bus_addr = 0;
    bus->bus_data = 0;
    bus->bus_shared = 0;
}

/**
 * @brief Arbitrates bus access using a Round-Robin algorithm.
 * * The arbiter ensures that every component (Cores 0-3 and Memory) receives
 * fair access to the bus.
 * * Priority Logic:
 * 1. If 'busy' is asserted, a multi-cycle operation is in progress; the
 * current grant is maintained and no new arbitration occurs.
 * 2. If the bus is free, the arbiter searches for the next request starting
 * from the agent immediately following the last served agent (RR index + 1).
 * 3. The first requesting agent found in the circular search wins the grant.
 */
void bus_arbitrate(Bus* bus, bool request_vector[5]) {
    /*
     * 1. BUSY CHECK
     * If the bus is currently executing a transaction (e.g., a multi-cycle flush),
     * we do not change the grant until the current owner releases the bus.
     */
    if (bus->busy) {
        return;
    }

    /*
     * 2. ROUND-ROBIN ARBITRATION
     * Scan the request_vector starting from (arbitration_rr_index + 1) mod 5.
     * Order of agents: Core 0, Core 1, Core 2, Core 3, Main Memory (4).
     */
    int count = 0;
    int candidate = (bus->arbitration_rr_index + 1) % 5;

    while (count < 5) {
        if (request_vector[candidate]) {
            // Found a valid request. Grant bus access to this candidate.
            bus->current_grant = candidate;
            bus->busy = true;

            /* * Update the Round-Robin index to the current winner to ensure
             * they have the lowest priority in the next arbitration cycle.
             * Note: In this implementation, the index is only updated if a Core wins.
             */
            if (candidate < 4) {
                bus->arbitration_rr_index = candidate;
            }
            return;
        }
        candidate = (candidate + 1) % 5;
        count++;
    }

    // No requests pending; reset the grant signal.
    bus->current_grant = -1;
}