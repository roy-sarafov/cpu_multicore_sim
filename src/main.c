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

    for (int i = 0; i < NUM_CORES; i++) {
        // ADD check for is_waiting_for_fill
        // This ensures we only request the bus AFTER the countdown is finished
        if (cores[i].stall && cores[i].ex_mem.valid && cores[i].l1_cache.is_waiting_for_fill) {
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
        // --- A. Start of Cycle ---
        bus_reset_signals(&bus);

        // --- B. Arbitration ---
        // Now checks requests generated in the PREVIOUS cycle
        bool requests[5];
        gather_bus_requests(cores, &main_memory, requests);
        bus_arbitrate(&bus, requests);

        // --- C. Drive Bus ---
        // ... (Your existing bus drive logic) ...
        bool any_hijack = false;
        for (int i=0; i<NUM_CORES; i++) {
             if (cores[i].l1_cache.is_flushing) any_hijack = true;
        }
        if (!any_hijack && bus.current_grant < 4 && bus.current_grant >= 0) {
            drive_bus_from_core(&cores[bus.current_grant], &bus);
            bus.busy = false;
        }

        // --- D & E. Memory/Cache Response (Dynamic Ordering) ---
        // ... (Your existing Snoop/Memory logic) ...
        if (bus.current_grant == 4) {
            memory_listen(&main_memory, &bus);
            for (int i = 0; i < NUM_CORES; i++) cache_snoop(&cores[i].l1_cache, &bus);
        } else {
            for (int i = 0; i < NUM_CORES; i++) cache_snoop(&cores[i].l1_cache, &bus);
            memory_listen(&main_memory, &bus);
        }

        // --- Latch Shared Signal ---
        if (bus.bus_shared) {
            // 1. If it's a normal request, update the originator (Standard Logic)
            if (bus.bus_origid < 4) {
                cores[bus.bus_origid].l1_cache.snoop_result_shared = true;
            }

            // 2. NEW: If this is a FLUSH, the "waiting" core also needs to know it's shared!
            if (bus.bus_cmd == BUS_CMD_FLUSH) {
                for (int i = 0; i < NUM_CORES; i++) {
                    // If Core i is waiting for this exact address, tell it "Shared=true"
                    if (cores[i].l1_cache.is_waiting_for_fill &&
                       (cores[i].l1_cache.pending_addr & ~0x7) == (bus.bus_addr & ~0x7)) {
                        cores[i].l1_cache.snoop_result_shared = true;
                       }
                }
            }
        }

        // --- G. Bus Trace ---
        if (bus_trace_file) {
            write_bus_trace(bus_trace_file, &bus, cycle);
        }

        // -----------------------------------------------------
        // MOVED TO END: Core Execution
        // -----------------------------------------------------
        // 1. Print Trace (Current State before execution changes it)
        // 2. Run Cycle (Generate Next State / Detect Misses)
        bool all_halted = true;
        for (int i = 0; i < NUM_CORES; i++) {
            if (cores[i].halted) continue;

            // Note: We write trace here so it captures the state *at the start* of the cycle
            write_core_trace(trace_files[i], &cores[i], cycle);

            core_cycle(&cores[i], &bus); // If miss happens here, Arbiter sees it NEXT loop

            if (!cores[i].halted) all_halted = false;
        }

        // --- H. End of Cycle Checks ---
        if (all_halted) active = false;

        cycle++;
        if (cycle > 500000) break;
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