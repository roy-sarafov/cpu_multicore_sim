# Multi-Core Cache Simulator (MIPS-like)

**Author:**    
**ID:**   
**Date:** 11/11/2024

## Project Overview

This project implements a cycle-accurate simulator for a multi-core processor system. The system consists of 4 cores, each with a private L1 cache, connected via a shared bus to a main memory. The simulator models a 5-stage MIPS-like pipeline, MESI cache coherence protocol, and Round-Robin bus arbitration.

The simulator is written in C and is designed to run assembly programs provided in a custom format. It generates detailed trace files for each core and the bus, as well as final memory and register dumps.

## Directory Structure

*   **`src/`**: Contains the C source code for the simulator.
    *   `main.c`: Entry point, simulation loop, and system orchestration.
    *   `core.c`: Implementation of the 5-stage pipeline (Fetch, Decode, Execute, Memory, WriteBack).
    *   `cache.c`: L1 Cache logic, MESI protocol state machine, and snooping.
    *   `bus.c`: Shared bus implementation with Round-Robin arbitration.
    *   `memory.c`: Main memory logic with simulated latency.
    *   `io_handler.c`: File I/O, argument parsing, and trace generation.
*   **`include/`**: Header files defining structs, constants, and function prototypes.
*   **`asm/`**: (Optional) Directory for assembly source files.
*   **`example/`**: Contains example input files (`imemX.txt`, `memin.txt`) and expected outputs.
*   **`counter/`**: Assembly programs for the "Shared Counter" task (Token Passing).
*   **`new_counter/`**: Assembly programs for the "Shared Counter" task (Modulo Check).
*   **`mulserial/`**: Assembly programs for Serial Matrix Multiplication.
*   **`mulparallel/`**: Assembly programs for Parallel Matrix Multiplication.
*   **`CMakeLists.txt`**: Build configuration for CMake.

## Compilation

The project uses CMake for building.

1.  **Create a build directory:**
    ```bash
    mkdir build
    cd build
    ```
2.  **Run CMake:**
    ```bash
    cmake ..
    ```
3.  **Compile:**
    ```bash
    make
    ```

This will generate an executable named `cpu_multicore_sim` (or similar, depending on CMake config).

## Usage

The simulator accepts command-line arguments specifying the input and output file paths. If no arguments are provided, it defaults to a standard set of filenames (useful for local testing).

**Command Line Syntax:**
```bash
./cpu_multicore_sim <imem0> <imem1> <imem2> <imem3> <memin> <memout> <regout0> <regout1> <regout2> <regout3> <core0trace> <core1trace> <core2trace> <core3trace> <bustrace> <dsram0> <dsram1> <dsram2> <dsram3> <tsram0> <tsram1> <tsram2> <tsram3> <stats0> <stats1> <stats2> <stats3>
```

**Arguments:**
1.  `imem0` - `imem3`: Paths to instruction memory files for Cores 0-3.
2.  `memin`: Path to the initial main memory content file.
3.  `memout`: Path to write the final main memory content.
4.  `regout0` - `regout3`: Paths to write the final register values for Cores 0-3.
5.  `core0trace` - `core3trace`: Paths to write the cycle-by-cycle trace for each core.
6.  `bustrace`: Path to write the bus transaction trace.
7.  `dsram0` - `dsram3`: Paths to write the final Data SRAM content.
8.  `tsram0` - `tsram3`: Paths to write the final Tag SRAM content (Tags + MESI state).
9.  `stats0` - `stats3`: Paths to write execution statistics (cycles, hits/misses, stalls).

**Example (using defaults):**
```bash
./cpu_multicore_sim
```
*(Requires input files `imem0.txt`, `memin.txt`, etc., to be present in the working directory)*

## System Architecture

### 1. Cores
*   **Pipeline:** 5-stage (Fetch, Decode, Execute, Memory, WriteBack).
*   **Hazard Handling:** Detects RAW hazards and inserts stalls (bubbles).
*   **Branching:** Resolves branches in the Decode stage.
*   **ISA:** Subset of MIPS (ADD, SUB, MUL, LW, SW, BEQ, BNE, etc.).

### 2. L1 Cache
*   **Organization:** Direct-Mapped, 64 Sets, 8 Words (32 bytes) per block.
*   **Coherence:** MESI Protocol (Modified, Exclusive, Shared, Invalid).
*   **Write Policy:** Write-Back, Write-Allocate.
*   **Snooping:** Monitors the bus for Read/ReadX requests to maintain coherence.
*   **Latency:** 1 cycle for Hit. Miss penalty depends on bus contention and memory latency.

### 3. Bus
*   **Arbitration:** Round-Robin (Core 0 -> 1 -> 2 -> 3 -> Memory).
*   **Transactions:**
    *   `BusRd`: Read request (Shared intent).
    *   `BusRdX`: Read request (Exclusive intent / Write Miss).
    *   `Flush`: Write-back of a block to memory (or cache-to-cache transfer).
*   **Shared Line:** Wired-OR signal used by snoopers to indicate they have a copy of the requested block.

### 4. Main Memory
*   **Size:** 2^20 words (1 MB).
*   **Latency:** 16 cycles for the first word of a block, 1 cycle for subsequent words (Burst).
*   **Behavior:** Serves read requests from the bus and accepts flush data.

## Assembly Programs

### Shared Counter
*   **Goal:** Four cores concurrently increment a shared counter in memory.
*   **Synchronization:** Uses a "Token Passing" algorithm (or Modulo check) to ensure mutual exclusion.
*   **Files:** `counter/imemX.asm` (Token), `new_counter/imemX.asm` (Modulo).

### Matrix Multiplication
*   **Goal:** Multiply two 16x16 matrices (A * B = C).
*   **Serial:** Core 0 performs the entire calculation. Cores 1-3 are idle. (`mulserial/`)
*   **Parallel:** The workload is divided among 4 cores (4 rows each). (`mulparallel/`)

## Output Files

*   **`coreXtrace.txt`**: Detailed pipeline state (PC, Instructions, Registers) for every cycle.
*   **`bustrace.txt`**: Log of all bus transactions (Cycle, Originator, Command, Address, Data, Shared).
*   **`dsramX.txt`**: Dump of the cache data array.
*   **`tsramX.txt`**: Dump of the cache tag array (including MESI bits).
*   **`statsX.txt`**: Summary metrics (Cycles, Instructions, Cache Hits/Misses, Stalls).
*   **`memout.txt`**: Final state of main memory.
*   **`regoutX.txt`**: Final values of registers R2-R15.
