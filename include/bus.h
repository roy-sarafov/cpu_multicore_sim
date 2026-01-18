#ifndef BUS_H
#define BUS_H

#include "global.h"

/*
 * Bus Commands
 * Defines the types of transactions that can occur on the bus.
 */
typedef enum {
    BUS_CMD_NO_CMD = 0, // No active command
    BUS_CMD_READ   = 1, // BusRd: Request to read a block (Shared intent)
    BUS_CMD_READX  = 2, // BusRdX: Request to read a block (Exclusive intent)
    BUS_CMD_FLUSH  = 3  // Flush: Writing a block back to memory (or to another cache)
} BusCmd;

/*
 * Bus State Structure
 * Represents the physical wires and internal state of the system bus.
 */
typedef struct {
    // --- Visible Signals (Wires) ---
    int bus_origid;      // ID of the component driving the bus (0-3=Cores, 4=Mem)
    BusCmd bus_cmd;      // Current command on the bus
    uint32_t bus_addr;   // Address being accessed
    uint32_t bus_data;   // Data being transferred (valid during Flush)
    int bus_shared;      // Shared signal (wired-OR), asserted by snoopers

    // --- Internal Arbiter State ---
    bool busy;                // True if a multi-cycle transaction is in progress
    int current_grant;        // ID of the component currently granted bus access
    int arbitration_rr_index; // Round-Robin pointer (last serviced agent)
    
    int memory_countdown;     // (Legacy/Unused) Timer for memory operations
} Bus;

/*
 * bus_init
 * Initializes the bus structure to default values.
 */
void bus_init(Bus *bus);

/*
 * bus_reset_signals
 * Clears the transient signals (cmd, addr, data, shared) at the start of a cycle.
 * Does NOT clear internal state like 'busy' or 'arbitration_rr_index'.
 */
void bus_reset_signals(Bus *bus);

/*
 * bus_arbitrate
 * Performs Round-Robin arbitration to select the next bus master.
 * Updates 'current_grant' based on the 'request_vector'.
 */
void bus_arbitrate(Bus *bus, bool request_vector[5]);

#endif
