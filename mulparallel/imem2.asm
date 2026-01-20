# Parallel Matrix Multiplication (Core 2)
# Rows 8 to 12

add $r2, $zero, $imm, 0     # Base A = 0
add $r3, $zero, $imm, 256   # Base B = 256
add $r4, $zero, $imm, 512   # Base C = 512
add $r5, $zero, $imm, 8     # i = 8 (Start Row)
add $r13, $zero, $imm, 12   # i Limit = 12 (End Row)
add $r14, $zero, $imm, 16   # j, k Limit = 16

Loop_I:
	beq $imm, $r5, $r13, End_I
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

	add $r6, $zero, $imm, 0         # j = 0

Loop_J:
	beq $imm, $r6, $r14, End_J
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

	add $r8, $zero, $imm, 0         # sum = 0
	add $r7, $zero, $imm, 0         # k = 0

Loop_K:
	beq $imm, $r7, $r14, End_K
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

	# Load A[i][k]
	sll $r9, $r5, $imm, 4
	add $r9, $r9, $r7, 0
	add $r9, $r9, $r2, 0
	lw $r10, $r9, $imm, 0

	# Load B[k][j]
	sll $r9, $r7, $imm, 4
	add $r9, $r9, $r6, 0
	add $r9, $r9, $r3, 0
	lw $r11, $r9, $imm, 0

	# Multiply and Accumulate
	mul $r12, $r10, $r11, 0
	add $r8, $r8, $r12, 0

	# Increment k
	add $r7, $r7, $imm, 1
	beq $imm, $zero, $zero, Loop_K
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

End_K:
	# Store C[i][j]
	sll $r9, $r5, $imm, 4
	add $r9, $r9, $r6, 0
	add $r9, $r9, $r4, 0
	sw $r8, $r9, $imm, 0

	# Increment j
	add $r6, $r6, $imm, 1
	beq $imm, $zero, $zero, Loop_J
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

End_J:
	# Increment i
	add $r5, $r5, $imm, 1
	beq $imm, $zero, $zero, Loop_I
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

End_I:
	halt $zero, $zero, $zero, 0