# ==============================================================================
# Project: Multi-Core Cache Simulator (MIPS-like)
# File:    imem0.asm (mulparallel - Core 0)
#
# Description:
# Parallel Matrix Multiplication (16x16).
# Core 0 is assigned to calculate Rows 0 through 3 of the result Matrix C.
# Matrix A starts at 0, Matrix B at 256, and Matrix C at 512.
# ==============================================================================

# --- PHASE 1: INITIALIZATION ---
# Set the row processing range for Core 0.
add $r2, $zero, $imm, 0         # Initialize i = 0 (Start Row for Core 0)
add $r12, $zero, $imm, 4        # Set Row Limit = 4 (Processes rows 0, 1, 2, 3)

loop_i:
    # Check if this core has completed its assigned row block.
    beq $imm, $r2, $r12, flush_start # If i reaches 4, jump to cache flush
    add $r3, $zero, $zero, 0    # Initialize j = 0 (Column index)

loop_j:
    add $r14, $zero, $imm, 16   # Set column boundary to 16
    beq $imm, $r3, $r14, next_i # If j reaches 16, move to the next row (i++)
    add $r4, $zero, $zero, 0    # Initialize k = 0 (Dot product iterator)
    add $r5, $zero, $zero, 0    # Initialize sum = 0 for current element C[i][j]

loop_k:
    add $r14, $zero, $imm, 16   # Set dot product boundary to 16
    beq $imm, $r4, $r14, store_c # If k reaches 16, dot product is complete
    add $zero, $zero, $zero, 0  # Pipeline NOP (Delay slot)

    # 1. Load element A[i][k] (Matrix A Base Address = 0)
    sll $r9, $r2, $imm, 4       # Calculate row offset: i * 16
    add $r9, $r9, $r4, 0        # Add column k: (i * 16) + k
    lw  $r9, $r9, $zero, 0      # Load A[i][k] into $r9

    # 2. Load element B[k][j] (Matrix B Base Address = 256)
    sll $r10, $r4, $imm, 4      # Calculate row offset: k * 16
    add $r10, $r10, $r3, 0      # Add column j: (k * 16) + j
    add $r10, $r10, $imm, 256   # Add Matrix B base offset (256)
    lw  $r10, $r10, $zero, 0    # Load B[k][j] into $r10

    # 3. Multiply and Accumulate
    mul $r13, $r9, $r10, 0      # Multiply partials: temp = A * B
    add $r5, $r5, $r13, 0       # sum = sum + temp

    add $r4, $r4, $imm, 1       # Increment k (k++)
    beq $imm, $zero, $zero, loop_k # Repeat inner loop
    add $zero, $zero, $zero, 0  # Pipeline NOP

store_c:
    # 4. Store the finalized sum into Matrix C (Matrix C Base Address = 512)
    sll $r8, $r2, $imm, 4       # row offset: i * 16
    add $r8, $r8, $r3, 0        # column offset: (i * 16) + j
    add $r8, $r8, $imm, 512     # Add Matrix C base offset (512)
    sw  $r5, $r8, $zero, 0      # Store the result word to memory

    add $r3, $r3, $imm, 1       # Increment j (j++)
    beq $imm, $zero, $zero, loop_j # Repeat middle loop
    add $zero, $zero, $zero, 0  # Pipeline NOP

next_i:
    add $r2, $r2, $imm, 1       # Increment i (i++)
    beq $imm, $zero, $zero, loop_i # Repeat outer loop
    add $zero, $zero, $zero, 0  # Pipeline NOP

# --- PHASE 2: CACHE FLUSH ---
# Ensure Core 0 writes back all 'Modified' blocks to memory before halting.
flush_start:
    add $r2, $zero, $zero, 0    # Reset pointer to 0
    add $r12, $zero, $imm, 256  # Flush through 256 words

flush_loop:
    beq $imm, $r2, $r12, halt_core # If range covered, exit
    add $zero, $zero, $zero, 0  # Pipeline NOP
    lw $r0, $r2, $zero, 0       # Trigger eviction via dummy load
    add $r2, $r2, $imm, 8       # Move to next block (8-word stride)
    beq $imm, $zero, $zero, flush_loop # Continue flushing
    add $zero, $zero, $zero, 0  # Pipeline NOP

halt_core:
    halt $zero, $zero, $zero, 0 # Terminate Core 0