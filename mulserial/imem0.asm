# Serial Matrix Multiplication (Core 0)
# Performs C = A * B for 16x16 matrices
#
# Memory Layout (Word Addressed):
#   Matrix A: 0x000 - 0x0FF (Size: 256)
#   Matrix B: 0x100 - 0x1FF (Size: 256)
#   Matrix C: 0x200 - 0x2FF (Size: 256)
#
# Register Mapping:
#   $r2  : Base Address of Matrix A
#   $r3  : Base Address of Matrix B
#   $r4  : Base Address of Matrix C
#   $r5  : i (Row index)
#   $r6  : j (Col index)
#   $r7  : k (Dot product index)
#   $r8  : sum (Accumulator)
#   $r13 : Loop Limit (16)
#   $r14 : Loop Limit (16)

add $r2, $zero, $imm, 0     # Base A = 0
add $r3, $zero, $imm, 256   # Base B = 256
add $r4, $zero, $imm, 512   # Base C = 512
add $r5, $zero, $imm, 0     # i = 0 (Start Row)
add $r13, $zero, $imm, 16   # i Limit = 16 (End Row)
add $r14, $zero, $imm, 16   # j, k Limit = 16

Loop_I:
	beq $imm, $r5, $r13, End_I      # If i == 16, finish
	add $zero, $zero, $zero, 0      # [DELAY SLOT] No-op

	add $r6, $zero, $imm, 0         # j = 0 (Reset Column)

Loop_J:
	beq $imm, $r6, $r14, End_J      # If j == 16, next row
	add $zero, $zero, $zero, 0      # [DELAY SLOT] No-op

	add $r8, $zero, $imm, 0         # sum = 0 (Reset Accumulator)
	add $r7, $zero, $imm, 0         # k = 0 (Reset Dot Product Index)

Loop_K:
	beq $imm, $r7, $r14, End_K      # If k == 16, store sum
	add $zero, $zero, $zero, 0      # [DELAY SLOT] No-op

	# Load A[i][k] -> Addr = BaseA + i*16 + k
	sll $r9, $r5, $imm, 4           # $r9 = i << 4 (i * 16)
	add $r9, $r9, $r7, 0            # $r9 = (i * 16) + k
	add $r9, $r9, $r2, 0            # $r9 = BaseA + Offset
	lw $r10, $r9, $imm, 0           # Load A[i][k] into $r10

	# Load B[k][j] -> Addr = BaseB + k*16 + j
	sll $r9, $r7, $imm, 4           # $r9 = k << 4 (k * 16)
	add $r9, $r9, $r6, 0            # $r9 = (k * 16) + j
	add $r9, $r9, $r3, 0            # $r9 = BaseB + Offset
	lw $r11, $r9, $imm, 0           # Load B[k][j] into $r11

	# Multiply and Accumulate
	mul $r12, $r10, $r11, 0         # $r12 = A[i][k] * B[k][j]
	add $r8, $r8, $r12, 0           # sum += $r12

	# Increment k
	add $r7, $r7, $imm, 1
	beq $imm, $zero, $zero, Loop_K  # Unconditional jump to Loop_K
	add $zero, $zero, $zero, 0      # [DELAY SLOT] No-op

End_K:
	# Store C[i][j] = sum -> Addr = BaseC + i*16 + j
	sll $r9, $r5, $imm, 4           # $r9 = i * 16
	add $r9, $r9, $r6, 0            # $r9 = (i * 16) + j
	add $r9, $r9, $r4, 0            # $r9 = BaseC + Offset
	sw $r8, $r9, $imm, 0            # Store sum into C[i][j]

	# Increment j
	add $r6, $r6, $imm, 1
	beq $imm, $zero, $zero, Loop_J  # Unconditional jump to Loop_J
	add $zero, $zero, $zero, 0      # [DELAY SLOT] No-op

End_J:
	# Increment i
	add $r5, $r5, $imm, 1
	beq $imm, $zero, $zero, Loop_I  # Unconditional jump to Loop_I
	add $zero, $zero, $zero, 0      # [DELAY SLOT] No-op

End_I:
	# ---------------------------------------------------------
	# FLUSH ROUTINE
	# ---------------------------------------------------------
	# Force eviction of Matrix C (0x200-0x2FF) by reading Matrix A (0x000-0x0FF).
	# Both map to Sets 0-31 in a 64-set direct-mapped cache.
	# We use $r5 as the address offset iterator and $r13 as the limit.

	add $r5, $zero, $imm, 0       # i = 0 (Start offset)
	add $r13, $zero, $imm, 256    # Limit = 256 words

FlushLoop:
	beq $imm, $r5, $r13, EndFlush   # If i >= 256, finish
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

	# Load from Matrix A (Base 0 + offset i)
	# This conflicts with Matrix C (Base 512 + offset i)
	# We load into $r0 (discard data) just to trigger the cache fill/eviction.
	lw $r0, $r5, $imm, 0

	# Increment by 8 (Block Size) to skip to the next cache line
	# We only need to touch one word per block to flush the whole block.
	add $r5, $r5, $imm, 8

	beq $imm, $zero, $zero, FlushLoop
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

EndFlush:
	halt $zero, $zero, $zero, 0