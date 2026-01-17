#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "io_handler.h"

// --- Helper: Parsing Logic ---

bool parse_arguments(int argc, char *argv[], SimFiles *files) {
    // If no arguments provided, use defaults (Section "Submission Requirements" implicitly allows defaults for local testing,
    // but the spec specifically lists default filenames to use if no args provided [cite: 71]).
    if (argc < 2) {
        // Defaults
        files->imem_paths[0] = "imem0.txt"; files->imem_paths[1] = "imem1.txt";
        files->imem_paths[2] = "imem2.txt"; files->imem_paths[3] = "imem3.txt";
        files->memin_path = "memin.txt";
        files->memout_path = "memout.txt";
        files->regout_paths[0] = "regout0.txt"; files->regout_paths[1] = "regout1.txt";
        files->regout_paths[2] = "regout2.txt"; files->regout_paths[3] = "regout3.txt";
        files->coretrace_paths[0] = "core0trace.txt"; files->coretrace_paths[1] = "core1trace.txt";
        files->coretrace_paths[2] = "core2trace.txt"; files->coretrace_paths[3] = "core3trace.txt";
        files->bustrace_path = "bustrace.txt";
        files->dsram_paths[0] = "dsram0.txt"; files->dsram_paths[1] = "dsram1.txt";
        files->dsram_paths[2] = "dsram2.txt"; files->dsram_paths[3] = "dsram3.txt";
        files->tsram_paths[0] = "tsram0.txt"; files->tsram_paths[1] = "tsram1.txt";
        files->tsram_paths[2] = "tsram2.txt"; files->tsram_paths[3] = "tsram3.txt";
        files->stats_paths[0] = "stats0.txt"; files->stats_paths[1] = "stats1.txt";
        files->stats_paths[2] = "stats2.txt"; files->stats_paths[3] = "stats3.txt";
        return true;
    }

    // Spec requires exactly 27 arguments if provided [cite: 69]
    // argv[0] is exe name. argv[1] starts the list.
    if (argc != 28) {
        printf("Error: Expected 27 arguments, got %d\n", argc - 1);
        return false;
    }

    int idx = 1;
    for (int i = 0; i < 4; i++) files->imem_paths[i] = argv[idx++];
    files->memin_path = argv[idx++];
    files->memout_path = argv[idx++];
    for (int i = 0; i < 4; i++) files->regout_paths[i] = argv[idx++];
    for (int i = 0; i < 4; i++) files->coretrace_paths[i] = argv[idx++];
    files->bustrace_path = argv[idx++];
    for (int i = 0; i < 4; i++) files->dsram_paths[i] = argv[idx++];
    for (int i = 0; i < 4; i++) files->tsram_paths[i] = argv[idx++];
    for (int i = 0; i < 4; i++) files->stats_paths[i] = argv[idx++];

    return true;
}

// --- Loading Functions ---

void load_imem_files(Core cores[], SimFiles *files) {
    for (int c = 0; c < NUM_CORES; c++) {
        FILE *fp = fopen(files->imem_paths[c], "r");
        if (!fp) {
            // Spec says "Assume input file is valid"[cite: 76], but safe to warn.
            // If file missing, memory stays 0 (NOPs).
            continue;
        }

        char line[64];
        int addr = 0;
        while (fgets(line, sizeof(line), fp) && addr < 1024) {
            // "8 hexadecimal digits" [cite: 74]
            cores[c].instruction_memory[addr] = (uint32_t)strtoul(line, NULL, 16);
            addr++;
        }
        fclose(fp);
    }
}

void load_memin_file(MainMemory *mem, SimFiles *files) {
    FILE *fp = fopen(files->memin_path, "r");
    if (!fp) return;

    char line[64];
    int addr = 0;
    while (fgets(line, sizeof(line), fp) && addr < MAIN_MEMORY_SIZE) {
        mem->data[addr] = (uint32_t)strtoul(line, NULL, 16);
        addr++;
    }
    fclose(fp);
}

// --- Trace Functions ---

void write_core_trace(FILE *fp, Core *core, int cycle) {
    fprintf(fp, "%d ", cycle);

    // 1. FETCH COLUMN: Print current PC (Waiting to be fetched)
    // If we are stalled/halted, this might stay same.
    // Logic: If halt detected and PC is 0, print bubbles?
    // Or just print PC. Standard is to print PC.
    // 1. FETCH COLUMN
    // If we have detected a HALT, the Fetch unit is off. Print "---".
    if (core->halt_detected) {
        fprintf(fp, "--- ");
    }
    else {
        fprintf(fp, "%03X ", core->pc & 0xFFF);
    }

    // 2. DECODE COLUMN: Print IF/ID Latch (Waiting to be decoded)
    // Check if empty/bubble (Instruction 0 usually implies bubble in this sim)
    if (core->if_id.Instruction == 0 && core->if_id.PC == 0) fprintf(fp, "--- ");
    else fprintf(fp, "%03X ", core->if_id.PC & 0xFFF);

    // 3. EXECUTE COLUMN: Print ID/EX Latch (Waiting to be executed)
    if (!core->id_ex.valid) fprintf(fp, "--- ");
    else fprintf(fp, "%03X ", core->id_ex.PC & 0xFFF);

    // 4. MEMORY COLUMN: Print EX/MEM Latch (Waiting for memory)
    if (!core->ex_mem.valid) fprintf(fp, "--- ");
    else fprintf(fp, "%03X ", core->ex_mem.PC & 0xFFF);

    // 5. WB COLUMN: Print MEM/WB Latch (Waiting for writeback)
    // CHANGE: We now read core->mem_wb directly!
    // Since we are PRE-execution, this latch has NOT been overwritten yet.
    if (!core->mem_wb.valid) {
        fprintf(fp, "--- ");
    } else {
        fprintf(fp, "%03X ", core->mem_wb.PC & 0xFFF);
    }

    // Print Registers
    for (int i = 2; i < 16; i++) {
        fprintf(fp, "%08X", core->regs[i]); // Note: Use current regs, not trace_regs
        if (i < 15) fprintf(fp, " ");
    }
    fprintf(fp, "\n");
}

void write_bus_trace(FILE *fp, Bus *bus, int cycle) {
    // "In every clock cycle where bus_cmd is not zero" [cite: 89]
    if (bus->bus_cmd == 0) return;

    // Format: CYCLE origid cmd addr data shared [cite: 90]
    // Widths: Dec, 1 hex, 1 hex, 6 hex, 8 hex, 1 hex [cite: 92]
    fprintf(fp, "%d %X %X %06X %08X %X\n",
            cycle,
            bus->bus_origid,
            bus->bus_cmd,
            bus->bus_addr & 0xFFFFFF, // Ensure 24-bit/6-hex fit
            bus->bus_data,
            bus->bus_shared
    );
}

// --- Final Output Functions ---

void write_regout_files(Core cores[], SimFiles *files) {
    for (int c = 0; c < NUM_CORES; c++) {
        FILE *fp = fopen(files->regout_paths[c], "w");
        if (!fp) continue;
        // Print R2-R15 [cite: 82]
        for (int i = 2; i < 16; i++) {
            fprintf(fp, "%08X\n", cores[c].regs[i]);
        }
        fclose(fp);
    }
}

void write_dsram_files(Core cores[], SimFiles *files) {
    for (int c = 0; c < NUM_CORES; c++) {
        FILE *fp = fopen(files->dsram_paths[c], "w");
        if (!fp) continue;

        // Print all 512 words.
        // Cache structure: 64 sets, 8 words per set.
        for (int s = 0; s < NUM_CACHE_SETS; s++) {
            for (int b = 0; b < BLOCK_SIZE; b++) {
                fprintf(fp, "%08X\n", cores[c].l1_cache.dsram[s][b]);
            }
        }
        fclose(fp);
    }
}

void write_tsram_files(Core cores[], SimFiles *files) {
    for (int c = 0; c < NUM_CORES; c++) {
        FILE *fp = fopen(files->tsram_paths[c], "w");
        if (!fp) continue;

        // TSRAM format: Bits 13:12 = MESI, Bits 11:0 = Tag [cite: 43]
        for (int s = 0; s < NUM_CACHE_SETS; s++) {
            uint32_t val = 0;
            val |= (cores[c].l1_cache.tsram[s].state << 12);
            val |= (cores[c].l1_cache.tsram[s].tag & 0xFFF);
            fprintf(fp, "%08X\n", val);
        }
        fclose(fp);
    }
}

void write_stats_files(Core cores[], SimFiles *files) {
    for (int c = 0; c < NUM_CORES; c++) {
        FILE *fp = fopen(files->stats_paths[c], "w");
        if (!fp) continue;

        // Fix: Read cache stats from l1_cache, but stalls/cycles from core.stats
        fprintf(fp, "cycles %d\n", cores[c].stats.cycles);
        fprintf(fp, "instructions %d\n", cores[c].stats.instructions);
        fprintf(fp, "read_hit %d\n", cores[c].l1_cache.read_hits);
        fprintf(fp, "write_hit %d\n", cores[c].l1_cache.write_hits);
        fprintf(fp, "read_miss %d\n", cores[c].l1_cache.read_miss);  // Singular, matches cache.c
        fprintf(fp, "write_miss %d\n", cores[c].l1_cache.write_miss); // Singular, matches cache.c
        fprintf(fp, "decode_stall %d\n", cores[c].stats.decode_stalls);
        fprintf(fp, "mem_stall %d\n", cores[c].stats.mem_stalls);

        fclose(fp);
    }
}

void write_memout_file(MainMemory *mem, SimFiles *files) {
    FILE *fp = fopen(files->memout_path, "w");
    if (!fp) return;

    // 1. Find the highest address that has non-zero data
    int max_addr = -1;
    for (int i = 0; i < MAIN_MEMORY_SIZE; i++) {
        if (mem->data[i] != 0) {
            max_addr = i;
        }
    }

    // 2. Print from 0 up to max_addr
    // If memory is completely empty (max_addr == -1), this loop won't run,
    // producing an empty file (which is valid for empty memory).
    for (int i = 0; i <= max_addr; i++) {
        fprintf(fp, "%08X\n", mem->data[i]);
    }

    fclose(fp);
}