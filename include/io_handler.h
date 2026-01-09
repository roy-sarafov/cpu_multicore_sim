#ifndef IO_HANDLER_H
#define IO_HANDLER_H

#include "global.h"
#include "core.h"
#include "memory.h"
#include "bus.h"

// Struct to hold all file paths parsed from argv
typedef struct {
    char *imem_paths[NUM_CORES];
    char *memin_path;
    char *memout_path;
    char *regout_paths[NUM_CORES];
    char *coretrace_paths[NUM_CORES];
    char *bustrace_path;
    char *dsram_paths[NUM_CORES];
    char *tsram_paths[NUM_CORES];
    char *stats_paths[NUM_CORES];
} SimFiles;

// Functions
bool parse_arguments(int argc, char *argv[], SimFiles *files);
void load_imem_files(Core cores[], SimFiles *files);
void load_memin_file(MainMemory *mem, SimFiles *files);

// Trace functions (called every cycle)
void write_core_trace(FILE *fp, Core *core, int cycle);
void write_bus_trace(FILE *fp, Bus *bus, int cycle);

// Final Dump functions (called at end of sim)
void write_regout_files(Core cores[], SimFiles *files);
void write_dsram_files(Core cores[], SimFiles *files);
void write_tsram_files(Core cores[], SimFiles *files);
void write_stats_files(Core cores[], SimFiles *files);
void write_memout_file(MainMemory *mem, SimFiles *files);

#endif // IO_HANDLER_H