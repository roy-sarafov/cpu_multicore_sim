#ifndef IO_HANDLER_H
#define IO_HANDLER_H

#include "global.h"
#include "core.h"
#include "memory.h"
#include "bus.h"

/*
 * SimFiles Structure
 * Holds the file paths for all input and output files used in the simulation.
 */
typedef struct {
    char *imem_paths[NUM_CORES];      // Paths to Instruction Memory files (imem0..3.txt)
    char *memin_path;                 // Path to Main Memory initialization file
    char *memout_path;                // Path to Main Memory dump file
    char *regout_paths[NUM_CORES];    // Paths to Register dump files
    char *coretrace_paths[NUM_CORES]; // Paths to Core Trace files
    char *bustrace_path;              // Path to Bus Trace file
    char *dsram_paths[NUM_CORES];     // Paths to DSRAM dump files
    char *tsram_paths[NUM_CORES];     // Paths to TSRAM dump files
    char *stats_paths[NUM_CORES];     // Paths to Statistics files
} SimFiles;

/*
 * parse_arguments
 * Parses command-line arguments and populates the SimFiles structure.
 * Returns true if successful, false if arguments are invalid.
 */
bool parse_arguments(int argc, char *argv[], SimFiles *files);

/*
 * load_imem_files
 * Reads the instruction memory files and loads them into each core's IMEM array.
 */
void load_imem_files(Core cores[], SimFiles *files);

/*
 * load_memin_file
 * Reads the memory initialization file and loads it into Main Memory.
 */
void load_memin_file(MainMemory *mem, SimFiles *files);

/*
 * write_core_trace
 * Appends a single line to the core trace file representing the current state.
 */
void write_core_trace(FILE *fp, Core *core, int cycle);

/*
 * write_bus_trace
 * Appends a single line to the bus trace file if a transaction occurred.
 */
void write_bus_trace(FILE *fp, Bus *bus, int cycle);

/*
 * Final Output Functions
 * These functions dump the final state of the system to files after simulation ends.
 */
void write_regout_files(Core cores[], SimFiles *files);
void write_dsram_files(Core cores[], SimFiles *files);
void write_tsram_files(Core cores[], SimFiles *files);
void write_stats_files(Core cores[], SimFiles *files);
void write_memout_file(MainMemory *mem, SimFiles *files);

#endif
