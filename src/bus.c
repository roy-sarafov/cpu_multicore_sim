#include <string.h>
#include "bus.h"

void bus_init(Bus *bus) {
    memset(bus, 0, sizeof(Bus));
    bus->arbitration_rr_index = 0; // Start Round-Robin with Core 0
}

void bus_reset_signals(Bus *bus) {
    // Reset the transient signals on the wires for the new cycle
    bus->bus_origid = 0;
    bus->bus_cmd = BUS_CMD_NO_CMD;
    bus->bus_addr = 0;
    bus->bus_data = 0;
    bus->bus_shared = 0; // Default to 0, snooping caches will pull this up to 1
}

// Round-Robin Arbitration:
// "The last core that received access is now the last priority" [cite: 49]
void bus_arbitrate(Bus *bus, bool request_vector[5]) {
    // If the bus is already busy with a transaction that isn't finished,
    // no arbitration happens.
    if (bus->busy) {
        return;
    }

    // Check requests starting from the core AFTER the last one served
    int count = 0;
    int candidate = (bus->arbitration_rr_index + 1) % 5;

    // We check 5 potential requesters (Cores 0-3, Memory=4)
    while (count < 5) {
        if (request_vector[candidate]) {
            // Grant bus to this candidate
            bus->current_grant = candidate;
            bus->busy = true; // Bus becomes occupied
            bus->arbitration_rr_index = candidate; // Update RR history
            return;
        }
        candidate = (candidate + 1) % 5;
        count++;
    }

    // No requests found
    bus->current_grant = -1;
}
