# ==============================================================================
# Project: Multi-Core Cache Simulator (MIPS-like)
# File:    imem1.asm (mulparallel - Core 1)
#
# Description:
# Parallel Matrix Multiplication (16x16).
# Core 1 is assigned to calculate Rows 4 through 7 of the result Matrix C.
# ==============================================================================

# --- PHASE 1: INITIALIZATION ---
# Set the row processing range for Core 1.
add $r2, $zero, $imm, 4         # Initialize i = 4 (Start Row for Core 1)
add $r12, $zero, $imm, 8        # Set Row Limit = 8 (Processes rows 4, 5, 6, 7)

loop_i:
    # Check for completion of the assigned row block.
    beq $imm, $r2, $r12, flush_start # If i reaches 8, jump to cache flush
    add $r3, $zero, $zero, 0    # Initialize j = 0 (Column index)

loop_j:
    add $r14, $zero, $imm, 16   # Set column boundary to 16
    beq $imm, $r3, $r14, next_i # If j reaches 16, move to next row (i++)
    add $r4, $zero, $zero, 0    # Initialize k = 0 (Dot product iterator)
    add $r5, $zero, $zero, 0    # Initialize sum = 0 for element C[i][j]

loop_k:
    add $r14, $zero, $imm, 16   # Set dot product boundary to 16
    beq $imm, $r4, $r14, store_c # If k reaches 16, dot product finished
    add $zero, $zero, $zero, 0  # Pipeline NOP

    # 1. Load A[i][k] (Matrix A Base Address = 0)
    sll $r9, $r2, $imm, 4       # Calculate row offset: i * 16
    add $r9, $r9, $r4, 0        # Add column k: (i * 16) + k
    lw  $r9, $r9, $zero, 0      # Load A[i][k] from memory

    # 2. Load B[k][j] (Matrix B Base Address = 256)
    sll $r10, $r4, $imm, 4      # Calculate row offset: k * 16
    add $r10, $r10, $r3, 0      # Add column j: (k * 16) + j
    add $r10, $r10, $imm, 256   # Add Matrix B base offset (256)
    lw  $r10, $r10, $zero, 0    # Load B[k][j] from memory

    # 3. Multiply and Accumulate
    mul $r13, $r9, $r10, 0      # Multiply: temp = A * B
    add $r5, $r5, $r13, 0       # sum = sum + temp

    add $r4, $r4, $imm, 1       # Increment k (k++)
    beq $imm, $zero, $zero, loop_k # Repeat inner loop
    add $zero, $zero, $zero, 0  # Pipeline NOP

store_c:
    # 4. Store finalized sum into Matrix C (Matrix C Base Address = 512)
    sll $r8, $r2, $imm, 4       # row offset: i * 16
    add $r8, $r8, $r3, 0        # column offset: (i * 16) + j
    add $r8, $r8, $imm, 512     # Add Matrix C base offset (512)
    sw  $r5, $r8, $zero, 0      # Write the final sum to memory

    add $r3, $r3, $imm, 1       # Increment j (j++)
    beq $imm, $zero, $zero, loop_j # Repeat middle loop
    add $zero, $zero, $zero, 0  # Pipeline NOP

next_i:
    add $r2, $r2, $imm, 1       # Increment i (i++)
    beq $imm, $zero, $zero, loop_i # Repeat outer loop
    add $zero, $zero, $zero, 0  # Pipeline NOP

# --- PHASE 2: CACHE FLUSH ---
flush_start:
    add $r2, $zero, $zero, 0    # Reset pointer to 0
    add $r12, $zero, $imm, 256  # Flush limit 256 words

flush_loop:
    beq $imm, $r2, $r12, halt_core # If finished, exit
    add $zero, $zero, $zero, 0  # Pipeline NOP
    lw $r0, $r2, $zero, 0       # Perform dummy load for cache eviction
    add $r2, $r2, $imm, 8       # Move 8 words forward
    beq $imm, $zero, $zero, flush_loop # Repeat flush
    add $zero, $zero, $zero, 0  # Pipeline NOP

halt_core:
    halt $zero, $zero, $zero, 0 # Terminate Core 1