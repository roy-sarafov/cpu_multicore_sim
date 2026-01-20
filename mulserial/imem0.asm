# FASTEST VERSION: Original ijk Logic + Flush Fix
# Registers:
# r2 = Base A, r3 = Base B, r4 = Base C
# r5 = i, r6 = j, r7 = k, r8 = Sum
# r13, r14 = Limits

# 1. SETUP
add $r2, $zero, $imm, 0     # Base A = 0
add $r3, $zero, $imm, 256   # Base B = 256
add $r4, $zero, $imm, 512   # Base C = 512
add $r5, $zero, $imm, 0     # i = 0
add $r13, $zero, $imm, 16   # Limit = 16
add $r14, $zero, $imm, 256  # Flush Limit

Loop_I:
    add $r6, $zero, $imm, 0         # j = 0

Loop_J:
    add $r8, $zero, $imm, 0         # sum = 0 (Accumulate in Register!)
    add $r7, $zero, $imm, 0         # k = 0

Loop_K:
    # Load A[i][k]
    sll $r9, $r5, $imm, 4           # i*16
    add $r9, $r9, $r7, 0            # + k
    add $r9, $r9, $r2, 0            # Base A
    lw $r10, $r9, $imm, 0

    # Load B[k][j]
    sll $r9, $r7, $imm, 4           # k*16
    add $r9, $r9, $r6, 0            # + j
    add $r9, $r9, $r3, 0            # Base B
    lw $r11, $r9, $imm, 0

    # Math
    mul $r12, $r10, $r11, 0
    add $r8, $r8, $r12, 0           # Accumulate in r8 (Fast!)

    # Increment k
    add $r7, $r7, $imm, 1
    bne $imm, $r7, $r13, Loop_K     # Branch with Delay Slot
    add $zero, $zero, $zero, 0

    # Store C[i][j] (Only ONCE per cell)
    sll $r9, $r5, $imm, 4
    add $r9, $r9, $r6, 0
    add $r9, $r9, $r4, 0
    sw $r8, $r9, $imm, 0

    # Increment j
    add $r6, $r6, $imm, 1
    bne $imm, $r6, $r13, Loop_J
    add $zero, $zero, $zero, 0

    # Increment i
    add $r5, $r5, $imm, 1
    bne $imm, $r5, $r13, Loop_I
    add $zero, $zero, $zero, 0

# ---------------------------------------------------------
# FLUSH ROUTINE (The Fix for Correct Output)
# ---------------------------------------------------------
add $r5, $zero, $imm, 0
FlushLoop:
    lw $r0, $r5, $imm, 0      # Read Matrix A to evict Matrix C
    add $r5, $r5, $imm, 8
    blt $imm, $r5, $r14, FlushLoop
    add $zero, $zero, $zero, 0

halt $zero, $zero, $zero, 0