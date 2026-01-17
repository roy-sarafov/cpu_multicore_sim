#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "global.h"
#include "core.h"
#include "bus.h"
#include "memory.h"
#include "io_handler.h"

// --- Helper: Bus Request Logic ---
// We need to check if any core *needs* the bus (is stalled at MEM stage)
// so we can tell the arbiter.
void gather_bus_requests(Core cores[], MainMemory *mem, bool requests[5]) {
    // Reset vector
    for (int i = 0; i < 5; i++) requests[i] = false;

    // RULE: If Memory is processing a read, NO CORE can request the bus.
    // Only Memory is allowed to request (to send the Flush).
    if (mem->processing_read) { // Use the flag you added to the struct
        requests[4] = true;
        return; // EXIT EARLY: Cores are blocked!
    }

    // Normal Logic (Same as before)
    for (int i = 0; i < NUM_CORES; i++) {
        if (cores[i].stall && cores[i].ex_mem.valid) {
            Opcode op = cores[i].ex_mem.Op;
            if (op == OP_LW || op == OP_SW) {
                requests[i] = true;
            }
        }
    }
}

// --- Helper: Drive Bus ---
// If a Core won arbitration, we must force it to put its command on the bus lines.
void drive_bus_from_core(Core *core, Bus *bus) {
    if (bus->current_grant != core->id) return;

    // Use the instruction sitting in EX_MEM latch
    EX_MEM_Latch *latch = &core->ex_mem;

    // Safety check
    if (!latch->valid) return;

    // Determine Command
    bus->bus_origid = core->id;
    bus->bus_addr = latch->ALUOutput; // Address calculated in EX

    if (latch->Op == OP_LW) {
        bus->bus_cmd = BUS_CMD_READ;
    } else if (latch->Op == OP_SW) {
        bus->bus_cmd = BUS_CMD_READX;
        // For Write Miss, we don't put data on bus yet (Write Allocate).
        // We read the block first.
    }
}

int main(int argc, char *argv[]) {
    // 1. Setup Input/Output Paths
    SimFiles files;
    if (!parse_arguments(argc, argv, &files)) {
        return 1;
    }

    // 2. Initialize Hardware
    Bus bus;
    bus_init(&bus);

    static MainMemory main_memory;
    memory_init(&main_memory);
    load_memin_file(&main_memory, &files);

    Core cores[NUM_CORES];
    for (int i = 0; i < NUM_CORES; i++) {
        core_init(&cores[i], i, files.imem_paths[i]);
    }

    // 3. Open Trace Files
    FILE *trace_files[NUM_CORES];
    FILE *bus_trace_file = fopen(files.bustrace_path, "w");

    for (int i = 0; i < NUM_CORES; i++) {
        trace_files[i] = fopen(files.coretrace_paths[i], "w");
        if (!trace_files[i]) {
            printf("Error opening trace file for core %d\n", i);
            return 1;
        }
    }

    // 4. Main Simulation Loop
    int cycle = 0;
    bool active = true;

    while (active) {

        // --- F. Core Execution ---
        bool all_halted = true;
        for (int i = 0; i < NUM_CORES; i++) {
            // IF ALREADY HALTED, SKIP EVERYTHING (Logic & Trace)
            if (cores[i].halted) {
                continue;
            }
            // Log Trace
            write_core_trace(trace_files[i], &cores[i], cycle);
            // call cycle
            core_cycle(&cores[i], &bus);




            if (!cores[i].halted) {
                all_halted = false;
            }
        }

        // --- A. Start of Cycle ---
        bus_reset_signals(&bus);

        // --- B. Arbitration ---
        bool requests[5];
        gather_bus_requests(cores, &main_memory, requests);
        bus_arbitrate(&bus, requests);

        // --- C. Drive Bus ---
        if (bus.current_grant < 4 && bus.current_grant >= 0) {
            drive_bus_from_core(&cores[bus.current_grant], &bus);

            // ADD THIS: Release the bus wires immediately after command is sent
            bus.busy = false;
        }

        // --- D & E. Dynamic Memory/Cache Ordering ---
        // If Memory has the grant, it is driving the bus (sending data).
        // It must run FIRST so caches can see the data.
        if (bus.current_grant == 4) {
            memory_listen(&main_memory, &bus);
            for (int i = 0; i < NUM_CORES; i++) {
                cache_snoop(&cores[i].l1_cache, &bus);
            }
        }
        // Otherwise (Core driving or Cache hijacking), Caches must run FIRST
        // to handle snooping and potentially override the bus with a Flush.
        else {
            for (int i = 0; i < NUM_CORES; i++) {
                cache_snoop(&cores[i].l1_cache, &bus);
            }
            memory_listen(&main_memory, &bus);
        }



        // --- NEW: Latch the Shared Signal ---
        // If a Read transaction just happened, the requester needs to know
        // if anyone else had the data (to decide Exclusive vs Shared).
        // New Version: Trust the wire. If shared is high, latch it.
        if (bus.bus_shared && bus.bus_origid < 4) {
            cores[bus.bus_origid].l1_cache.snoop_result_shared = true;
        }

        // --- G. Bus Trace ---
        if (bus_trace_file) {
            write_bus_trace(bus_trace_file, &bus, cycle);
        }

        // --- H. End of Cycle Checks ---
        if (all_halted) {
            active = false;
        }

        cycle++;
        // Safety Break (optional)
        if (cycle > 500000) {
            printf("Timeout: Exceeded 500,000 cycles. Infinite loop?\n");
            break;
        }
    }

    // 5. Cleanup and Dump Files
    write_regout_files(cores, &files);
    write_dsram_files(cores, &files);
    write_tsram_files(cores, &files);
    write_stats_files(cores, &files);
    write_memout_file(&main_memory, &files);

    // Close files
    for (int i = 0; i < NUM_CORES; i++) {
        if (trace_files[i]) fclose(trace_files[i]);
    }
    if (bus_trace_file) fclose(bus_trace_file);

    printf("Simulation completed successfully in %d cycles.\n", cycle);
    return 0;
}