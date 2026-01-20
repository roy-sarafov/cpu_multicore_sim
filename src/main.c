/*
 * Project: Multi-Core Cache Simulator (MIPS-like)
 * File:    main.c
 * Author:
 * ID:
 * Date:    11/11/2024
 *
 * Description:
 * The main entry point of the simulation. Orchestrates the cycle-by-cycle
 * execution of Cores, Bus, and Memory. Manages the main simulation loop,
 * arbitration logic, and trace generation.
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "global.h"
#include "core.h"
#include "bus.h"
#include "memory.h"
#include "io_handler.h"

void gather_bus_requests(Core cores[], MainMemory *mem, bool requests[5]) {
    for (int i = 0; i < 5; i++) requests[i] = false;

    /*
     * 1. MEMORY PRIORITY
     * If Memory is currently processing a read (latency countdown active),
     * it effectively holds a request to send data back when ready.
     * (Though arbitration usually handles this via the 'busy' flag or specific grant).
     */
    if (mem->processing_read) {
        requests[4] = true;
        return; 
    }

    /*
     * 2. CORE REQUESTS
     * Check each core to see if it needs the bus.
     * A core needs the bus if:
     * - It is stalled at the Memory stage.
     * - It has a valid instruction (Load/Store).
     * - It has a pending address (miss detected).
     * - It is NOT already waiting for a fill (request already sent).
     */
    for (int i = 0; i < NUM_CORES; i++) {
        bool needs_bus = cores[i].stall &&
                         cores[i].ex_mem.valid &&
                         cores[i].l1_cache.pending_addr != 0xFFFFFFFF && 
                         !cores[i].l1_cache.is_waiting_for_fill;

        // <--- ADD THIS BLOCK --->
        // Also request bus if we need to evict dirty data
        if (cores[i].l1_cache.eviction_pending) {
            needs_bus = true;
        }
        // <----------------------->

        if (needs_bus) {
            requests[i] = true;
        }
    }
}


void drive_bus_from_core(Core *core, Bus *bus) {
    if (bus->current_grant != core->id) return;

    // 1. Handle Eviction Grant
    if (core->l1_cache.eviction_pending) {
        // Transition from "Pending" to "Active Flushing"
        core->l1_cache.is_flushing = true;
        core->l1_cache.eviction_pending = false;
        core->l1_cache.flush_offset = 0;

        // We don't drive the bus data here directly.
        // Setting 'is_flushing' will cause cache_snoop (in the next phase)
        // to drive the bus with BUS_CMD_FLUSH.
        return;
    }

    EX_MEM_Latch *latch = &core->ex_mem;
    if (!latch->valid) return;

    bus->bus_origid = core->id;
    bus->bus_addr = latch->ALUOutput; 

    if (latch->Op == OP_LW) {
        bus->bus_cmd = BUS_CMD_READ;
    } else if (latch->Op == OP_SW) {
        bus->bus_cmd = BUS_CMD_READX;
    }

    /*
     * MARK AS WAITING
     * Once the request is on the bus, the core enters "Waiting for Fill" state.
     * It will stop requesting the bus and start listening for data.
     */
    core->l1_cache.is_waiting_for_fill = true;
}

int main(int argc, char *argv[]) {
    
    // 1. SETUP
    SimFiles files;
    if (!parse_arguments(argc, argv, &files)) return 1;

    Bus bus;
    bus_init(&bus);

    static MainMemory main_memory;
    memory_init(&main_memory);
    load_memin_file(&main_memory, &files);

    Core cores[NUM_CORES];
    for (int i = 0; i < NUM_CORES; i++) {
        core_init(&cores[i], i, files.imem_paths[i]);
    }

    FILE *trace_files[NUM_CORES];
    FILE *bus_trace_file = fopen(files.bustrace_path, "w");
    for (int i = 0; i < NUM_CORES; i++) {
        trace_files[i] = fopen(files.coretrace_paths[i], "w");
    }

    int cycle = 0;
    bool active = true;

    // 2. MAIN LOOP
    while (active) {
        
        // A. Reset Bus Signals (Start of Cycle)
        bus_reset_signals(&bus);

        // B. Arbitration Phase
        bool requests[5];
        gather_bus_requests(cores, &main_memory, requests);
        bus_arbitrate(&bus, requests);

        // C. Bus Driving Phase
        // Check if any core is "hijacking" the bus for a Flush (highest priority)
        bool any_hijack = false;
        for (int i=0; i<NUM_CORES; i++) {
             if (cores[i].l1_cache.is_flushing) any_hijack = true;
        }

        // If no flush is happening, let the granted core drive the bus
        if (!any_hijack && bus.current_grant < 4 && bus.current_grant >= 0) {
            drive_bus_from_core(&cores[bus.current_grant], &bus);
            bus.busy = false; 
        }

        // D. Snooping / Memory Response Phase
        // Order matters: If Memory is driving, Cores snoop. If Core is driving, Memory listens.
        if (bus.current_grant == 4) {
            memory_listen(&main_memory, &bus);
            for (int i = 0; i < NUM_CORES; i++) cache_snoop(&cores[i].l1_cache, &bus);
        } else {
            for (int i = 0; i < NUM_CORES; i++) cache_snoop(&cores[i].l1_cache, &bus);
            memory_listen(&main_memory, &bus);
        }

        // E. Shared Signal Propagation
        if (bus.bus_shared) {
            if (bus.bus_origid < 4) {
                cores[bus.bus_origid].l1_cache.snoop_result_shared = true;
            }
            // Special case: If data is being flushed, the waiting core also needs to know it's shared
            if (bus.bus_cmd == BUS_CMD_FLUSH) {
                for (int i = 0; i < NUM_CORES; i++) {
                    if (cores[i].l1_cache.is_waiting_for_fill &&
                       (cores[i].l1_cache.pending_addr & ~0x7) == (bus.bus_addr & ~0x7)) {
                        cores[i].l1_cache.snoop_result_shared = true;
                       }
                }
            }
        }

        // F. Trace Generation
        if (bus_trace_file) {
            write_bus_trace(bus_trace_file, &bus, cycle);
        }

        // G. Core Execution Phase
        bool all_halted = true;
        for (int i = 0; i < NUM_CORES; i++) {
            if (cores[i].halted) continue;
            write_core_trace(trace_files[i], &cores[i], cycle);
            core_cycle(&cores[i], &bus); 
            if (!cores[i].halted) all_halted = false;
        }

        // H. End of Cycle Checks
        if (all_halted) active = false;
        cycle++;
        if (cycle > 500000) break;
    }

    // 3. FINAL OUTPUT
    write_regout_files(cores, &files);
    write_dsram_files(cores, &files);
    write_tsram_files(cores, &files);
    write_stats_files(cores, &files);
    write_memout_file(&main_memory, &files);

    for (int i = 0; i < NUM_CORES; i++) if (trace_files[i]) fclose(trace_files[i]);
    if (bus_trace_file) fclose(bus_trace_file);

    printf("Simulation completed successfully in %d cycles.\n", cycle);
    return 0;
}
