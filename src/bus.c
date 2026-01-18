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
    if (bus->busy) {
        return;
    }

    int count = 0;
    int candidate = (bus->arbitration_rr_index + 1) % 5;

    while (count < 5) {
        if (request_vector[candidate]) {
            bus->current_grant = candidate;
            bus->busy = true;
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
