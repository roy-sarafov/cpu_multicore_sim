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

    // 1. Check Cores (0-3)
    for (int i = 0; i < NUM_CORES; i++) {
        // If core is stalled specifically in MEM stage (and not halted), it needs bus
        // We look at the EX_MEM latch to see if it's a valid Load/Store
        if (cores[i].stall && cores[i].ex_mem.valid) {
            Opcode op = cores[i].ex_mem.Op;
            if (op == OP_LW || op == OP_SW) {
                // It's a memory stall, so it requests the bus
                requests[i] = true;
            }
        }
    }

    // 2. Check Main Memory (4)
    // Memory assumes it always wants to reply if it has data ready.
    // In our simple memory.c, we can assume if it's processing, it requests.
    // However, memory.c usually responds immediately when latency timer hits 0.
    // For this simulation, we'll let memory.c internal logic handle the "Grant" check
    // inside memory_listen, but we need to pass a request 'true' here to ensure
    // the arbiter considers it?
    // Actually, memory usually has highest priority or specific slots.
    // Our Round Robin includes ID 4. So we need to know if Mem is ready to send.
    // We'll assume Memory requests if it has a pending response.
    // (This requires peeking into memory state, or we just set requests[4] = true
    // if we know it's active. For simplicity, we can let memory always request
    // if it has a transaction pending. Since we don't have a public 'is_pending' flag
    // in the header, we might need to add one or just assume logic handles it).
    requests[4] = true; // Simpler approach: Memory always "wants" to reply if it can.
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

    MainMemory main_memory;
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
        bool requests[5];
        gather_bus_requests(cores, &main_memory, requests);
        bus_arbitrate(&bus, requests);

        // --- C. Drive Bus (If a Core won) ---
        // If Memory won, it drives inside 'memory_listen' later.
        if (bus.current_grant < 4 && bus.current_grant >= 0) {
            drive_bus_from_core(&cores[bus.current_grant], &bus);
        }

        // --- D. Memory Response ---
        // Memory listens to bus commands and responds (or drives bus if it won grant)
        memory_listen(&main_memory, &bus);

        // --- E. Snooping ---
        // All caches see the bus signals and update MESI states
        for (int i = 0; i < NUM_CORES; i++) {
            cache_snoop(&cores[i].l1_cache, &bus);
        }

        // --- F. Core Execution ---
        bool all_halted = true;
        for (int i = 0; i < NUM_CORES; i++) {
            core_cycle(&cores[i], &bus);

            // Log Trace
            write_core_trace(trace_files[i], &cores[i], cycle);

            if (!cores[i].halted) {
                all_halted = false;
            }
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