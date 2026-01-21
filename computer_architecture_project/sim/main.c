/*
 * Project: Multi-Core Cache Simulator (MIPS-like)
 * File:    main.c
 * * Description:
 * This is the central engine of the simulation. It models a synchronous hardware
 * environment by dividing each clock cycle into discrete phases: Arbitration,
 * Bus Driving, Snooping, and Pipeline Execution.
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "global.h"
#include "core.h"
#include "bus.h"
#include "memory.h"
#include "io_handler.h"

 /**
  * @brief Scans the system for bus access requests.
  * * Cores request the bus for:
  * - L1 Cache Misses (requiring a Fill).
  * - Write Misses or Upgrades (BusRdX).
  * - Conflict Evictions (requiring a Flush of dirty data).
  */
void gather_bus_requests(Core cores[], MainMemory* mem, bool requests[5]) {
    for (int i = 0; i < 5; i++) requests[i] = false;

    /* 1. MEMORY PRIORITY
     * Memory requests the bus when its latency timer has expired and it is
     * ready to return data to a core.
     */
    if (mem->processing_read) {
        requests[4] = true;
        return;
    }

    /* 2. CORE REQUESTS */
    for (int i = 0; i < NUM_CORES; i++) {
        // A core needs the bus if it is stalled at the MEM stage and needs a fill
        bool needs_bus = cores[i].stall &&
            cores[i].ex_mem.valid &&
            cores[i].l1_cache.pending_addr != 0xFFFFFFFF &&
            !cores[i].l1_cache.is_waiting_for_fill;

        // Higher priority: Core needs the bus to evict a Modified block
        if (cores[i].l1_cache.eviction_pending) {
            needs_bus = true;
        }

        if (needs_bus) {
            requests[i] = true;
        }
    }
}

/**
 * @brief Translates core state into bus signal transitions.
 * * If a core is granted the bus, it drives its request (Read/ReadX)
 * onto the bus wires.
 */
void drive_bus_from_core(Core* core, Bus* bus) {
    if (bus->current_grant != core->id) return;

    /* 1. Handle Eviction Grant */
    if (core->l1_cache.eviction_pending) {
        // Transition internal cache state to actively flushing
        core->l1_cache.is_flushing = true;
        core->l1_cache.eviction_pending = false;
        core->l1_cache.flush_offset = 0;
        return;
    }

    EX_MEM_Latch* latch = &core->ex_mem;
    if (!latch->valid) return;

    /* 2. Drive Physical Signals */
    bus->bus_origid = core->id;
    bus->bus_addr = latch->ALUOutput;

    if (latch->Op == OP_LW) {
        bus->bus_cmd = BUS_CMD_READ;
    }
    else if (latch->Op == OP_SW) {
        bus->bus_cmd = BUS_CMD_READX;
    }

    /* * Once driven, the core enters a wait state. It will stop requesting
     * the bus and wait for either a snoop hit (from another cache)
     * or a memory response.
     */
    core->l1_cache.is_waiting_for_fill = true;
}

/**
 * @brief Main simulation entry point and loop.
 */
int main(int argc, char* argv[]) {

    /* 1. SYSTEM INITIALIZATION */
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

    // Open trace files
    FILE* trace_files[NUM_CORES];
    FILE* bus_trace_file = fopen(files.bustrace_path, "w");
    for (int i = 0; i < NUM_CORES; i++) {
        trace_files[i] = fopen(files.coretrace_paths[i], "w");
    }

    int cycle = 0;
    bool active = true;

    /* 2. CLOCK CYCLE LOOP */
    while (active) {

        // Phase A: Reset transient bus wires
        bus_reset_signals(&bus);

        // Phase B: Bus Arbitration
        bool requests[5];
        gather_bus_requests(cores, &main_memory, requests);
        bus_arbitrate(&bus, requests);

        // Phase C: Bus Driving
        bool any_hijack = false;
        for (int i = 0; i < NUM_CORES; i++) {
            if (cores[i].l1_cache.is_flushing) any_hijack = true;
        }

        // Only allow core request if no cache is currently flushing (data transfer)
        if (!any_hijack && bus.current_grant < 4 && bus.current_grant >= 0) {
            drive_bus_from_core(&cores[bus.current_grant], &bus);
            bus.busy = false;
        }

        // Phase D: Snoop / Response Logic
        if (bus.current_grant == 4) {
            // Memory is driving; Cores listen
            memory_listen(&main_memory, &bus);
            for (int i = 0; i < NUM_CORES; i++) cache_snoop(&cores[i].l1_cache, &bus);
        }
        else {
            // Cores may be driving; Memory and other Cores listen
            for (int i = 0; i < NUM_CORES; i++) cache_snoop(&cores[i].l1_cache, &bus);
            memory_listen(&main_memory, &bus);
        }

        // Phase E: Global 'Shared' Wire Propagation
        if (bus.bus_shared) {
            if (bus.bus_origid < 4) {
                cores[bus.bus_origid].l1_cache.snoop_result_shared = true;
            }
            // If data is currently flying on the bus, the requester latches the shared bit
            if (bus.bus_cmd == BUS_CMD_FLUSH) {
                for (int i = 0; i < NUM_CORES; i++) {
                    if (cores[i].l1_cache.is_waiting_for_fill &&
                        (cores[i].l1_cache.pending_addr & ~0x7) == (bus.bus_addr & ~0x7)) {
                        cores[i].l1_cache.snoop_result_shared = true;
                    }
                }
            }
        }

        // Phase F: Logging and Tracing
        if (bus_trace_file) {
            write_bus_trace(bus_trace_file, &bus, cycle);
        }

        // Phase G: Architecture State Transition (Clock Edge)
        bool all_halted = true;
        for (int i = 0; i < NUM_CORES; i++) {
            if (cores[i].halted) continue;
            write_core_trace(trace_files[i], &cores[i], cycle);
            core_cycle(&cores[i], &bus);
            if (!cores[i].halted) all_halted = false;
        }

        // Phase H: Loop Exit Conditions
        if (all_halted) active = false;
        cycle++;
        if (cycle > 500000) break; // Safety timeout
    }

    /* 3. POST-SIMULATION STATE DUMPS */
    write_regout_files(cores, &files);
    write_dsram_files(cores, &files);
    write_tsram_files(cores, &files);
    write_stats_files(cores, &files);
    write_memout_file(&main_memory, &files);

    // Cleanup
    for (int i = 0; i < NUM_CORES; i++) if (trace_files[i]) fclose(trace_files[i]);
    if (bus_trace_file) fclose(bus_trace_file);

    printf("Simulation completed successfully in %d cycles.\n", cycle);
    return 0;
}