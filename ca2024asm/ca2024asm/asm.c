#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "project.h"

int imem[IMEM_SIZE];
int dmem[DMEM_SIZE];

// registers
int R[16];

// files
FILE *fp_asm, *fp_imemout, *fp_dmemout;

// opcode names
char op_name[][10] = { "add", "sub", "and", "or", "xor", "mul", "sll", "sra", "srl", "beq", "bne", "blt", "bgt", "ble", "bge", "jal", "lw", "sw", "ll", "sc", "halt" };

// register names
char reg_name[][10] = { "$zero", "$imm", "$v0", "$a0", "$a1", "$t0", "$t1", "$t2", "$t3", "$s0", "$s1", "$s2", "$gp", "$sp", "$fp", "$ra" };
char reg_altname[][10] = { "$zero", "$imm", "$r2", "$r3", "$r4", "$r5", "$r6", "$r7", "$r8", "$r9", "$r10", "$r11", "$r12", "$r13", "$r14", "$r15" };


char jumplabels[IMEM_SIZE][50];
char labels[IMEM_SIZE][50];

int main(int argc, char *argv[])
{
	int i, last;
	int inst, op, rd, rs, rt, imm, PC;
	int count = 0, halt = 0;
	char *p, *q;
	int addr, data;
	char line[500];

	// check that we have 3 cmd line parameters
	if (argc != 4) {
		printf("usage: asm program.asm imem.txt dmem.txt\n");
		exit(1);
	}


	// open files
	fp_asm = fopen(argv[1], "rt");
	fp_imemout = fopen(argv[2], "wt");
	fp_dmemout = fopen(argv[3], "wt");
	if (!fp_asm || !fp_imemout || !fp_dmemout) {
		printf("ERROR: couldn't open files\n");
		exit(1);
	}

	// zero memory
	memset(imem, 0, IMEM_SIZE * sizeof(int));
	memset(dmem, 0, DMEM_SIZE * sizeof(int));

	for (i = 0; i < IMEM_SIZE; i++) {
		jumplabels[i][0] = 0;
		labels[i][0] = 0;
	}

	PC = 0;
	while (1) {
		if (feof(fp_asm))
			break;
		if (fgets(line, 500, fp_asm) == NULL)
			break;

		printf("\nline: %s", line);

		// remove comment
		p = strchr(line, '#');
		if (p != NULL)
			*p = 0;

		// get next token
		p = strtok(line, "\t ,");
		if (p == NULL)
			continue;
		printf("next token: %s\n", p);

		if (strcmp(p, "") == 0)
			continue;

		// check if label
		q = strchr(p, ':');
		if (q != NULL) {
			// store label
			strcpy(labels[PC], p);
			q = strchr(labels[PC], ':');
			if (q == NULL) {
				printf("ERROR matching label\n");
				exit(1);
			}
			*q = 0;
			printf("matched label %s at PC %d\n", labels[PC], PC);

			// get next token
			p = strtok(NULL, "\t ,");
			if (p == NULL)
				continue;
			printf("next token: %s\n", p);
		}

		// check for .word
		if (strcmp(p, ".word") == 0) {
			// get next token
			p = strtok(NULL, "\t ,");
			if (p == NULL)
				continue;
			printf("next token: %s\n", p);
			if (p[0] == '0' && p[1] == 'x')
				sscanf(p + 2, "%x", &addr);
			else
				sscanf(p, "%d", &addr);

			// get next token
			p = strtok(NULL, "\t ,");
			if (p == NULL)
				continue;
			printf("next token: %s\n", p);
			if (p[0] == '0' && p[1] == 'x')
				sscanf(p + 2, "%x", &data);
			else
				sscanf(p, "%d", &data);

			if (addr < 0 || addr >= DMEM_SIZE) {
				printf("ERROR: address 0x%x out of range\n", addr);
				exit(1);
			}
			printf("setting dmem[0x%x] = 0x%x\n", addr, data);
			dmem[addr] = data;
			continue;
		}
					
		// otherwise parse opcode
		for (op = 0; op < 21; op++)
			if (strcmp(p, op_name[op]) == 0)
				break;
		if (op == 21) {
			printf("ERROR: unsupported opcode %s\n", p);
			exit(1);
		}
		printf("matched opcode %d (%s)\n", op, op_name[op]);

		// parse rd
		p = strtok(NULL, "\t ,");
		if (p == NULL)
			continue;
		printf("next token: %s\n", p);
		for (rd = 0; rd < 16; rd++) {
			if (strcmp(p, reg_name[rd]) == 0)
				break;
			if (strcmp(p, reg_altname[rd]) == 0)
				break;
		}
		if (rd == 16)
			continue;
		printf("rd: matched register %d (%s %s)\n", rd, reg_name[rd], reg_altname[rd]);

		// parse rs
		p = strtok(NULL, "\t ,");
		if (p == NULL)
			continue;
		printf("next token: %s\n", p);
		for (rs = 0; rs < 16; rs++) {
			if (strcmp(p, reg_name[rs]) == 0)
				break;
			if (strcmp(p, reg_altname[rs]) == 0)
				break;
		}
		if (rs == 16)
			continue;
		printf("rs: matched register %d (%s %s)\n", rs, reg_name[rs], reg_altname[rs]);

		// parse rt
		p = strtok(NULL, "\t ,");
		if (p == NULL)
			continue;
		printf("next token: %s\n", p);
		for (rt = 0; rt < 16; rt++) {
			if (strcmp(p, reg_name[rt]) == 0)
				break;
			if (strcmp(p, reg_altname[rt]) == 0)
				break;
		}
		if (rt == 16)
			continue;
		printf("rt: matched register %d (%s %s)\n", rt, reg_name[rt], reg_altname[rt]);

		// parse imm
		p = strtok(NULL, "\t ,");
		if (p == NULL)
			continue;
		printf("next token: %s\n", p);
		if (p[0] == '0' && p[1] == 'x')
			sscanf(p + 2, "%x", &imm);
		else if (isalpha(*p)) {
			printf("saving jumplabels[%d] = %s\n", PC, p);
			strcpy(jumplabels[PC], p);
			q = &jumplabels[PC][0];
			while (*q) {
//				printf("*q == %x\n", *q);
				if (*q == 'n' || *q == 0xd || *q == 0xa) {
					*q = 0;
					break;
				}
				q++;
			}
			imm = 0;
		} else
			sscanf(p, "%d", &imm);
		imm = sbs(imm, 11, 0);
		printf("imm: matched 0x%04x\n", imm);

		inst = (op << 24) | (rd << 20) | (rs << 16) | (rt << 12) | imm;
		printf("--> inst is mem[%d] = %08X\n", PC, inst);
		imem[PC++] = inst;

	}
	// fix labels
	for (PC = 0; PC < IMEM_SIZE; PC++) {
		if (jumplabels[PC][0] != 0) {
			for (i = 0; i < IMEM_SIZE; i++) {
//				printf("PC %d, i %d, jumplables[PC] %s (length %d), labels[i] %s (length %d)\n", PC, i, jumplabels[PC], strlen(jumplabels[PC]), labels[i], strlen(labels[i]));
				if (strcmp(jumplabels[PC], labels[i]) == 0) {
					printf("matched label %s from PC 0x%x to 0x%x\n", labels[i], PC, i);
					imem[PC] |= i;
					break;
				}
			}
			if (i == IMEM_SIZE) {
				printf("ERROR: couldn't find label %s referenced at PC %d\n", jumplabels[PC], PC);
				exit(1);
			}
		}
	}

	// print imem memory
	last = IMEM_SIZE - 1;
	while (last >= 0 && imem[last] == 0)
		last--;
	for (i = 0; i <= last; i++)
		fprintf(fp_imemout, "%08X\n", imem[i]);

	// print dmem memory
	last = DMEM_SIZE - 1;
	while (last >= 0 && dmem[last] == 0)
		last--;
	for (i = 0; i <= last; i++)
		fprintf(fp_dmemout, "%08X\n", dmem[i]);

	// close files
	fclose(fp_asm);
	fclose(fp_imemout);
	fclose(fp_dmemout);
	
	return 0;
}
