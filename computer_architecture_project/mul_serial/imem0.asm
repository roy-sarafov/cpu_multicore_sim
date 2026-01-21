# ==============================================================================
# Project: Multi-Core Cache Simulator (MIPS-like)
# File:    imem0.asm (mulserial)
# 
# Description:
# Performs a serial 16x16 Matrix Multiplication (C = A * B) using Core 0.
# The algorithm utilizes an optimized ijk loop structure, accumulating 
# partial sums in a register ($r8) to minimize cache write traffic.
# Includes a final cache flush routine to ensure memory consistency.
# ==============================================================================

# ------------------------------------------------------------------------------
# REGISTER ALLOCATION
# ------------------------------------------------------------------------------
# $r2: Base Address of Matrix A (Address 0)
# $r3: Base Address of Matrix B (Address 256)
# $r4: Base Address of Matrix C (Address 512)
# $r5: Outer Loop Index 'i' (Row of A)
# $r6: Middle Loop Index 'j' (Column of B)
# $r7: Inner Loop Index  'k' (Dot Product iterator)
# $r8: Accumulator for the Dot Product (partial sum)
# $r13: Loop Limit (Matrix Dimension = 16)
# $r14: Flush Limit (Total words to read to clear cache)
# $r9, $r10, $r11, $r12: Temporary calculation and data registers
# ------------------------------------------------------------------------------

# --- PHASE 1: SETUP ---
add $r2, $zero, $imm, 0     # Initialize Base A pointer to address 0
add $r3, $zero, $imm, 256   # Initialize Base B pointer to address 256
add $r4, $zero, $imm, 512   # Initialize Base C pointer to address 512
add $r5, $zero, $imm, 0     # Initialize i = 0
add $r13, $zero, $imm, 16   # Set loop boundary to 16 (for 16x16 matrices)
add $r14, $zero, $imm, 256  # Set flush boundary to 256 words

# --- PHASE 2: MATRIX MULTIPLICATION (ijk loops) ---
Loop_I:
    add $r6, $zero, $imm, 0         # Reset j = 0 for each new row of A

Loop_J:
    add $r8, $zero, $imm, 0         # Reset accumulator (sum) for C[i][j]
    add $r7, $zero, $imm, 0         # Reset k = 0 for dot product

Loop_K:
    # 1. Calculate address and Load A[i][k]
    sll $r9, $r5, $imm, 4           # Calculate i * 16 (shift row index)
    add $r9, $r9, $r7, 0            # Add k (column index)
    add $r9, $r9, $r2, 0            # Add base offset of A
    lw $r10, $r9, $imm, 0           # Load A[i][k] into $r10

    # 2. Calculate address and Load B[k][j]
    sll $r9, $r7, $imm, 4           # Calculate k * 16 (shift row index)
    add $r9, $r9, $r6, 0            # Add j (column index)
    add $r9, $r9, $r3, 0            # Add base offset of B
    lw $r11, $r9, $imm, 0           # Load B[k][j] into $r11

    # 3. Multiply and Accumulate
    mul $r12, $r10, $r11, 0         # $r12 = A[i][k] * B[k][j]
    add $r8, $r8, $r12, 0           # sum += result (accumulate in register)

    # 4. Inner Loop Control (k)
    add $r7, $r7, $imm, 1           # k++
    bne $imm, $r7, $r13, Loop_K     # Repeat Inner Loop until k == 16
    add $zero, $zero, $zero, 0      # Delay slot

    # 5. Store Final Dot Product to C[i][j]
    sll $r9, $r5, $imm, 4           # Calculate i * 16
    add $r9, $r9, $r6, 0            # Add j
    add $r9, $r9, $r4, 0            # Add base offset of C
    sw $r8, $r9, $imm, 0            # Write finalized sum to Memory

    # 6. Middle Loop Control (j)
    add $r6, $r6, $imm, 1           # j++
    bne $imm, $r6, $r13, Loop_J     # Repeat Middle Loop until j == 16
    add $zero, $zero, $zero, 0      # Delay slot

    # 7. Outer Loop Control (i)
    add $r5, $r5, $imm, 1           # i++
    bne $imm, $r5, $r13, Loop_I     # Repeat Outer Loop until i == 16
    add $zero, $zero, $zero, 0      # Delay slot

# --- PHASE 3: CACHE FLUSH ROUTINE ---
# This routine forces the L1 cache to write back all 'Modified' blocks 
# belonging to Matrix C to main memory. By reading Matrix A (256 words), 
# the cache lines are evicted and the simulator's memory consistency is ensured.
add $r5, $zero, $imm, 0
FlushLoop:
    lw $r0, $r5, $imm, 0            # Dummy read from Matrix A to force eviction
    add $r5, $r5, $imm, 8           # Increment by block size (8 words)
    blt $imm, $r5, $r14, FlushLoop  # Continue until 256 words are processed
    add $zero, $zero, $zero, 0      # Delay slot

halt $zero, $zero, $zero, 0         # Stop Execution