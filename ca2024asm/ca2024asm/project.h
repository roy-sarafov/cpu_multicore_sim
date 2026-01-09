// opcodes
#define OP_ADD 0
#define OP_SUB 1
#define OP_AND 2
#define OP_OR  3
#define OP_XOR 4
#define OP_MUL 5
#define OP_SLL 6
#define OP_SRA 7
#define OP_SRL 8
#define OP_BEQ 9
#define OP_BNE 10
#define OP_BLT 11
#define OP_BGT 12
#define OP_BLE 13
#define OP_BGE 14
#define OP_JAL 15
#define OP_LW  16
#define OP_SW  17
#define OP_LL  18
#define OP_SC  19
#define OP_HALT 20

// instruction memory
#define IMEM_SIZE (1 << 10)
#define DMEM_SIZE (1 << 20)

// extract single bit
static inline int sb(int x, int bit)
{
	return (x >> bit) & 1;
}

// extract multiple bits
static inline int sbs(int x, int msb, int lsb)
{
	if (msb == 31 && lsb == 0)
		return x;
	return (x >> lsb) & ((1 << (msb - lsb + 1)) - 1);
}
