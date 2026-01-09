#ifndef BUS_H
#define BUS_H

#include "global.h"

// Bus Commands [cite: 48]
typedef enum {
    BUS_CMD_NO_CMD = 0,
    BUS_CMD_READ   = 1, // BusRd
    BUS_CMD_READX  = 2, // BusRdX
    BUS_CMD_FLUSH  = 3  // Flush
} BusCmd;

// Bus State
typedef struct {
    // Current transaction signals (visible on the bus lines)
    int bus_origid;      // 0-3: Cores, 4: Main Memory [cite: 48]
    BusCmd bus_cmd;      // Command Type
    uint32_t bus_addr;   // Word Address
    uint32_t bus_data;   // Word Data
    int bus_shared;      // 0 or 1 [cite: 48]

    // Internal Arbitration Logic
    bool busy;           // Is a transaction currently occupying the bus?
    int current_grant;   // Who currently has the bus grant?
    int arbitration_rr_index; // For Round-Robin logic (last granted core) [cite: 49]
    
    // Memory latency handling
    int memory_countdown; // Main memory takes 16 cycles for first word [cite: 55]
    
} Bus;

void bus_init(Bus *bus);
void bus_reset_signals(Bus *bus); // Clears lines for the new cycle
void bus_arbitrate(Bus *bus, bool request_vector[5]); // Decides who goes next

#endif // BUS_H