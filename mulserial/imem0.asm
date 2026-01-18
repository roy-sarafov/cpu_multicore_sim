# ==============================================================================
# Project: Multi-Core Cache Simulator (MIPS-like)
# File:    mulserial/imem0.asm
# Author:
# ID:
# Date:    11/11/2024
#
# Description:
# Serial Matrix Multiplication (16x16) executed by Core 0.
# Calculates C = A * B.
# A is at 0x0, B is at 0x100 (256), C is at 0x200 (512).
# ==============================================================================

# ------------------------------------------------------------------------------
# REGISTER MAP
# ------------------------------------------------------------------------------
# $r2:  i (Row Index)
# $r3:  j (Column Index)
# $r4:  k (Dot Product Index)
# $r5:  Base Address of Matrix A (0)
# $r6:  Base Address of Matrix B (256)
# $r7:  Base Address of Matrix C (512)
# $r8:  Accumulator (Sum for C[i][j])
# $r9:  Matrix Dimension (16)
# $r10: Temporary Address Calculation (A & C)
# $r11: Loaded Value from A
# $r12: Temporary Address Calculation (B)
# $r13: Loaded Value from B
# $r14: Product (A * B)
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# INITIALIZATION
# ------------------------------------------------------------------------------
    add $r5, $zero, $imm, 0     # Base A = 0
    add $r6, $zero, $imm, 256   # Base B = 0x100
    add $r7, $zero, $imm, 512   # Base C = 0x200
    add $r9, $zero, $imm, 16    # Size = 16

    add $r2, $zero, $zero, 0    # i = 0

# ------------------------------------------------------------------------------
# OUTER LOOP (Rows)
# ------------------------------------------------------------------------------
LoopI:
    beq $r2, $r9, $imm, Done    # if i == 16, done
    add $r3, $zero, $zero, 0    # j = 0

# ------------------------------------------------------------------------------
# MIDDLE LOOP (Columns)
# ------------------------------------------------------------------------------
LoopJ:
    beq $r3, $r9, $imm, NextI   # if j == 16, next row
    add $r4, $zero, $zero, 0    # k = 0
    add $r8, $zero, $zero, 0    # sum = 0

# ------------------------------------------------------------------------------
# INNER LOOP (Dot Product)
# ------------------------------------------------------------------------------
LoopK:
    beq $r4, $r9, $imm, WriteC  # if k == 16, write result

    # Calculate Addr A[i][k] = BaseA + i*16 + k
    mul $r10, $r2, $imm, 16     # r10 = i * 16
    add $r10, $r10, $r4         # r10 = i*16 + k
    add $r10, $r10, $r5         # r10 = BaseA + offset
    lw  $r11, $r10, $imm, 0     # r11 = A[i][k]

    # Calculate Addr B[k][j] = BaseB + k*16 + j
    mul $r12, $r4, $imm, 16     # r12 = k * 16
    add $r12, $r12, $r3         # r12 = k*16 + j
    add $r12, $r12, $r6         # r12 = BaseB + offset
    lw  $r13, $r12, $imm, 0     # r13 = B[k][j]

    # Multiply and Accumulate
    mul $r14, $r11, $r13        # r14 = A * B
    add $r8, $r8, $r14          # sum += r14

    add $r4, $r4, $imm, 1       # k++
    beq $zero, $zero, $imm, LoopK

# ------------------------------------------------------------------------------
# WRITE RESULT
# ------------------------------------------------------------------------------
WriteC:
    # Calculate Addr C[i][j] = BaseC + i*16 + j
    mul $r10, $r2, $imm, 16
    add $r10, $r10, $r3
    add $r10, $r10, $r7
    sw  $r8, $r10, $imm, 0      # C[i][j] = sum

    add $r3, $r3, $imm, 1       # j++
    beq $zero, $zero, $imm, LoopJ

# ------------------------------------------------------------------------------
# NEXT ROW
# ------------------------------------------------------------------------------
NextI:
    add $r2, $r2, $imm, 1       # i++
    beq $zero, $zero, $imm, LoopI

# ------------------------------------------------------------------------------
# TERMINATION
# ------------------------------------------------------------------------------
Done:
    halt $zero, $zero, $zero, 0
