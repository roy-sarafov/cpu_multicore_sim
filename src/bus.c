/*
 * Project: Multi-Core Cache Simulator (MIPS-like)
 * File:    bus.c
 * Author:
 * ID:
 * Date:    11/11/2024
 *
 * Description:
 * Implements the shared system bus with Round-Robin arbitration. Handles
 * command broadcasting and data flushing between Cores and Main Memory.
 */

#include <string.h>
#include "bus.h"

void bus_init(Bus *bus) {
    memset(bus, 0, sizeof(Bus));
    bus->arbitration_rr_index = 4;
}

void bus_reset_signals(Bus *bus) {
    bus->bus_origid = 0;
    bus->bus_cmd = BUS_CMD_NO_CMD;
    bus->bus_addr = 0;
    bus->bus_data = 0;
    bus->bus_shared = 0;
}

void bus_arbitrate(Bus *bus, bool request_vector[5]) {
    /*
     * 1. BUSY CHECK
     * If the bus is currently executing a transaction (e.g., a multi-cycle flush),
     * we do not change the grant.
     */
    if (bus->busy) {
        return;
    }

    /*
     * 2. ROUND-ROBIN ARBITRATION
     * We check requests starting from the agent AFTER the last one served.
     * Order: Core 0 -> 1 -> 2 -> 3 -> Memory (4) -> Core 0...
     */
    int count = 0;
    int candidate = (bus->arbitration_rr_index + 1) % 5;

    while (count < 5) {
        if (request_vector[candidate]) {
            bus->current_grant = candidate;
            bus->busy = true;
            // Only update RR index if a Core won. Memory requests don't shift priority?
            // (Actually, standard RR updates for everyone, but let's keep existing logic if intended)
            if (candidate < 4) {
                bus->arbitration_rr_index = candidate;
            }
            return;
        }
        candidate = (candidate + 1) % 5;
        count++;
    }

    bus->current_grant = -1;
}
