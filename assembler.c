#define _CRT_SECURE_NO_WARNINGS // For Windows compatibility
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_LINE_LEN 512
#define MAX_LABELS 512
#define MAX_LABEL_LEN 64

// --- Data Structures ---

typedef struct {
    char name[MAX_LABEL_LEN];
    int address; // PC address
} Label;

Label symbol_table[MAX_LABELS];
int label_count = 0;

typedef struct {
    char *name;
    int opcode;
} Instruction;

// Opcode Map (Matches Spec Page 4-5)
Instruction instructions[] = {
    {"add", 0}, {"sub", 1}, {"and", 2}, {"or", 3}, {"xor", 4},
    {"mul", 5}, {"sll", 6}, {"sra", 7}, {"srl", 8}, {"beq", 9},
    {"bne", 10}, {"blt", 11}, {"bgt", 12}, {"ble", 13}, {"bge", 14},
    {"jal", 15}, {"lw", 16}, {"sw", 17}, {"halt", 20},
    {NULL, -1}
};

// --- Helpers ---

void add_label(char *name, int address) {
    // Remove colon if present
    char clean_name[MAX_LABEL_LEN];
    strcpy(clean_name, name);
    char *colon = strchr(clean_name, ':');
    if (colon) *colon = '\0';

    strcpy(symbol_table[label_count].name, clean_name);
    symbol_table[label_count].address = address;
    label_count++;
}

int get_label_address(char *name) {
    for (int i = 0; i < label_count; i++) {
        if (strcmp(symbol_table[i].name, name) == 0) {
            return symbol_table[i].address;
        }
    }
    return -1;
}

int get_opcode(char *str) {
    for (int i = 0; instructions[i].name != NULL; i++) {
        if (strcmp(instructions[i].name, str) == 0) {
            return instructions[i].opcode;
        }
    }
    // Support .word for data
    if (strcmp(str, ".word") == 0) return -2;
    return -1;
}

// Parses registers: $zero, $imm, $r2, etc.
int parse_register(char *str) {
    if (!str) return 0;

    // Check named registers
    if (strstr(str, "$zero")) return 0;
    if (strstr(str, "$imm"))  return 1;

    // Check standard $rX or rX
    char *p = str;
    while (*p && !isdigit(*p)) p++; // Skip non-digits

    if (*p == '\0') return 0; // Default if parsing fails
    return atoi(p);
}

// Parses immediate: hex, dec, or label
int parse_immediate(char *str, int current_pc) {
    if (!str) return 0;

    // Check if it's a number (starts with digit or - or 0x)
    // Note: strtol handles 0x automatically
    char *endptr;
    long val = strtol(str, &endptr, 0);

    // If strtol consumed the whole string (or up to whitespace), it's a number
    if (endptr != str && (*endptr == '\0' || isspace((unsigned char)*endptr))) {
        return (int)val;
    }

    // Otherwise, assume it is a LABEL
    // Remove potential trailing comma or newline if parser failed to split perfectly
    // (Our tokenizer usually handles this, but safety first)
    int addr = get_label_address(str);
    if (addr == -1) {
        printf("Error: Undefined label '%s' used at PC %d\n", str, current_pc);
        exit(1);
    }
    return addr;
}

// --- Main Processing ---

void process_file(char *input_path, char *output_path) {
    FILE *in = fopen(input_path, "r");
    if (!in) { printf("Error opening %s\n", input_path); return; }

    // --- Pass 1: Symbol Table ---
    char line[MAX_LINE_LEN];
    int pc = 0;

    while (fgets(line, sizeof(line), in)) {
        // Find comment start
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';

        // Tokenize
        // Delimiters: space, tab, newline, carriage return, comma
        char *token = strtok(line, " \t\n\r,");

        while (token != NULL) {
            // Check if label (ends with :)
            if (strchr(token, ':')) {
                add_label(token, pc);
                token = strtok(NULL, " \t\n\r,");
                continue; // Check next token on same line
            }

            // If token exists and isn't a label, it's an instruction
            // (Assuming one instruction per line)
            if (token) {
                pc++;
                break; // Move to next line
            }
            token = strtok(NULL, " \t\n\r,");
        }
    }

    // --- Pass 2: Code Generation ---
    rewind(in);
    FILE *out = fopen(output_path, "w");
    if (!out) { printf("Error opening output %s\n", output_path); fclose(in); return; }

    pc = 0;
    while (fgets(line, sizeof(line), in)) {
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';

        char *token = strtok(line, " \t\n\r,");
        if (!token) continue; // Empty line

        // Handle Label at start of line
        if (strchr(token, ':')) {
            token = strtok(NULL, " \t\n\r,");
            if (!token) continue; // Label on its own line
        }

        // Handle .word directive
        if (strcmp(token, ".word") == 0) {
            char *val_str = strtok(NULL, " \t\n\r,");
            uint32_t val = (uint32_t)strtol(val_str, NULL, 0);
            fprintf(out, "%08X\n", val);
            pc++;
            continue;
        }

        // Get Opcode
        int opcode = get_opcode(token);
        if (opcode == -1) {
            printf("Error: Unknown opcode '%s' at PC %d\n", token, pc);
            exit(1);
        }

        // Parse 4 Arguments: rd, rs, rt, imm
        char *arg1 = strtok(NULL, " \t\n\r,");
        char *arg2 = strtok(NULL, " \t\n\r,");
        char *arg3 = strtok(NULL, " \t\n\r,");
        char *arg4 = strtok(NULL, " \t\n\r,");

        // Defaults if arguments are missing (e.g. halt)
        int rd = parse_register(arg1);
        int rs = parse_register(arg2);
        int rt = parse_register(arg3);
        int imm = parse_immediate(arg4, pc); // Pass PC for error reporting

        // Encoding:
        // 31:24 Opcode (8)
        // 23:20 rd (4)
        // 19:16 rs (4)
        // 15:12 rt (4)
        // 11:0  imm (12)

        uint32_t inst = 0;
        inst |= (opcode & 0xFF) << 24;
        inst |= (rd & 0xF) << 20;
        inst |= (rs & 0xF) << 16;
        inst |= (rt & 0xF) << 12;
        inst |= (imm & 0xFFF); // Mask to 12 bits

        fprintf(out, "%08X\n", inst);
        pc++;
    }

    fclose(in);
    fclose(out);
    printf("Assembled %s -> %s\n", input_path, output_path);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: assembler <input.asm> <output.txt>\n");
        return 1;
    }
    process_file(argv[1], argv[2]);
    return 0;
}