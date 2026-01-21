#ifndef BUS_H
#define BUS_H

#include "global.h"

/**
 * @brief Bus Command Definitions
 * * These constants represent the command signals driven on the bus to facilitate
 * the snooping coherence protocol (e.g., MESI/MSI).
 */
typedef enum {
    BUS_CMD_NO_CMD = 0, /**< No active transaction on the bus. */
    BUS_CMD_READ = 1, /**< BusRd: Issued when a core needs a block for reading. */
    BUS_CMD_READX = 2, /**< BusRdX: Issued when a core needs to write to a block it doesn't own exclusively. */
    BUS_CMD_FLUSH = 3  /**< Flush: Issued when a block is being written back to main memory. */
} BusCmd;

/**
 * @brief Bus State Structure
 * * Encapsulates the state of the shared system bus, including both the physical
 * signal lines (wires) and the internal state required for arbitration logic.
 */
typedef struct {
    /* --- Visible Bus Signals (Physical Wires) --- */

    int bus_origid;      /**< ID of the component currently driving the bus (0-3: Cores, 4: Main Memory). */
    BusCmd bus_cmd;      /**< The command currently being broadcasted on the bus. */
    uint32_t bus_addr;   /**< The memory address associated with the current bus transaction. */
    uint32_t bus_data;   /**< The data word being transferred (primarily used during Flush operations). */
    int bus_shared;      /**< Shared signal (wired-OR); asserted by snooping caches to indicate they hold the block. */

    /* --- Internal Arbiter & Controller State --- */

    bool busy;                /**< High if a multi-cycle transaction is currently occupying the bus. */
    int current_grant;        /**< ID of the agent (0-4) currently granted permission to drive the bus. */
    int arbitration_rr_index; /**< Round-Robin pointer tracking the last agent to successfully win arbitration. */

    int memory_countdown;     /**< (Unused) Placeholder for modeling memory latency. */
} Bus;

/**
 * @brief Initializes the bus structure.
 * * Sets all signals to zero and initializes the Round-Robin arbiter.
 * * @param bus Pointer to the Bus structure to be initialized.
 */
void bus_init(Bus* bus);

/**
 * @brief Resets transient bus signals.
 * * Clears the visible signal lines (cmd, addr, data, shared) at the beginning
 * of a clock cycle. This does not affect persistent state like 'busy' or the arbiter index.
 * * @param bus Pointer to the Bus structure.
 */
void bus_reset_signals(Bus* bus);

/**
 * @brief Performs bus arbitration logic.
 * * Uses a Round-Robin policy to select the next bus master from the request_vector.
 * If the bus is currently 'busy' with a transaction, the grant remains unchanged.
 * * @param bus Pointer to the Bus structure.
 * @param request_vector Array of booleans where index 0-3 are Cores and index 4 is Memory.
 */
void bus_arbitrate(Bus* bus, bool request_vector[5]);

#endif