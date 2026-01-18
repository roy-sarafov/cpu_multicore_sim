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

    if (mem->processing_read) { 
        requests[4] = true;
        return; 
    }

    for (int i = 0; i < NUM_CORES; i++) {
        
        
        bool needs_bus = cores[i].stall && 
                         cores[i].ex_mem.valid &&
                         cores[i].l1_cache.pending_addr != 0xFFFFFFFF && 
                         !cores[i].l1_cache.is_waiting_for_fill;

        if (needs_bus) {
            requests[i] = true;
        }
    }
}


void drive_bus_from_core(Core *core, Bus *bus) {
    if (bus->current_grant != core->id) return;

    EX_MEM_Latch *latch = &core->ex_mem;
    if (!latch->valid) return;

    bus->bus_origid = core->id;
    bus->bus_addr = latch->ALUOutput; 

    if (latch->Op == OP_LW) {
        bus->bus_cmd = BUS_CMD_READ;
    } else if (latch->Op == OP_SW) {
        bus->bus_cmd = BUS_CMD_READX;
    }

    
    
    
    core->l1_cache.is_waiting_for_fill = true;
}

int main(int argc, char *argv[]) {
    
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

    while (active) {
        
        bus_reset_signals(&bus);

        
        bool requests[5];
        gather_bus_requests(cores, &main_memory, requests);
        bus_arbitrate(&bus, requests);

        
        bool any_hijack = false;
        for (int i=0; i<NUM_CORES; i++) {
             if (cores[i].l1_cache.is_flushing) any_hijack = true;
        }
        
        
        if (!any_hijack && bus.current_grant < 4 && bus.current_grant >= 0) {
            drive_bus_from_core(&cores[bus.current_grant], &bus);
            bus.busy = false; 
        }

        
        
        
        if (bus.current_grant == 4) {
            memory_listen(&main_memory, &bus);
            for (int i = 0; i < NUM_CORES; i++) cache_snoop(&cores[i].l1_cache, &bus);
        } else {
            for (int i = 0; i < NUM_CORES; i++) cache_snoop(&cores[i].l1_cache, &bus);
            memory_listen(&main_memory, &bus);
        }

        if (bus.bus_shared) {
            if (bus.bus_origid < 4) {
                cores[bus.bus_origid].l1_cache.snoop_result_shared = true;
            }
            if (bus.bus_cmd == BUS_CMD_FLUSH) {
                for (int i = 0; i < NUM_CORES; i++) {
                    if (cores[i].l1_cache.is_waiting_for_fill &&
                       (cores[i].l1_cache.pending_addr & ~0x7) == (bus.bus_addr & ~0x7)) {
                        cores[i].l1_cache.snoop_result_shared = true;
                       }
                }
            }
        }

        if (bus_trace_file) {
            write_bus_trace(bus_trace_file, &bus, cycle);
        }

        bool all_halted = true;
        for (int i = 0; i < NUM_CORES; i++) {
            if (cores[i].halted) continue;
            write_core_trace(trace_files[i], &cores[i], cycle);
            core_cycle(&cores[i], &bus); 
            if (!cores[i].halted) all_halted = false;
        }

        if (all_halted) active = false;
        cycle++;
        if (cycle > 500000) break;
    }

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