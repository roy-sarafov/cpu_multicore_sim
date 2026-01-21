#ifndef IO_HANDLER_H
#define IO_HANDLER_H

#include "global.h"
#include "core.h"
#include "memory.h"
#include "bus.h"

/**
 * @brief Simulation File Path Container
 * * Stores all filesystem paths for input initialization files and output
 * reports generated during the simulation.
 */
typedef struct {
    char* imem_paths[NUM_CORES];      /**< Paths to instruction memory hex files (imem0..3.txt). */
    char* memin_path;                 /**< Path to the initial main memory state file. */
    char* memout_path;                /**< Path for the final main memory state dump. */
    char* regout_paths[NUM_CORES];    /**< Paths for final register state dumps for each core. */
    char* coretrace_paths[NUM_CORES]; /**< Paths for cycle-by-cycle execution traces per core. */
    char* bustrace_path;              /**< Path for the shared bus transaction trace. */
    char* dsram_paths[NUM_CORES];     /**< Paths for final Data SRAM (L1) contents. */
    char* tsram_paths[NUM_CORES];     /**< Paths for final Tag SRAM (L1) contents. */
    char* stats_paths[NUM_CORES];     /**< Paths for performance counter reports. */
} SimFiles;

/**
 * @brief Parses Command Line Arguments.
 * * Maps command line strings to the SimFiles structure. Supports a default
 * mode (no args) for local testing or a strict 27-argument mode for evaluation.
 * * @return true if paths are successfully mapped, false if arguments are missing.
 */
bool parse_arguments(int argc, char* argv[], SimFiles* files);

/**
 * @brief Loads Instruction Memory.
 * * Reads hex-formatted instruction files into each core's local IMEM array.
 */
void load_imem_files(Core cores[], SimFiles* files);

/**
 * @brief Loads Main Memory.
 * * Initializes the system's global memory from the specified input file.
 */
void load_memin_file(MainMemory* mem, SimFiles* files);

/**
 * @brief Records a core's cycle state.
 * * Writes the PC of each pipeline stage and the current register values to the trace.
 * * @param fp Pointer to the core's trace file.
 * @param core Pointer to the core being traced.
 * @param cycle The current global clock cycle.
 */
void write_core_trace(FILE* fp, Core* core, int cycle);

/**
 * @brief Records bus activity.
 * * If a command is active on the bus, logs the transaction details (ID, CMD, ADDR, DATA).
 */
void write_bus_trace(FILE* fp, Bus* bus, int cycle);

/* --- Final State Output Functions --- */

/** Dumps final R2-R15 register values for all cores. */
void write_regout_files(Core cores[], SimFiles* files);

/** Dumps the contents of the Data SRAM (Cache lines) for all cores. */
void write_dsram_files(Core cores[], SimFiles* files);

/** Dumps the Tag SRAM, including MESI states and address tags. */
void write_tsram_files(Core cores[], SimFiles* files);

/** Writes performance metrics (hits, misses, stalls) to text files. */
void write_stats_files(Core cores[], SimFiles* files);

/** Dumps the final state of main memory up to the last non-zero address. */
void write_memout_file(MainMemory* mem, SimFiles* files);

#endif