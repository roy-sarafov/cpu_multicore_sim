# ==============================================================================
# Project: Multi-Core Cache Simulator (MIPS-like)
# File:    imem3.asm
#
# Description:
# Implementation for Core 3. Coordinates via (Counter % 4 == 3).
# ==============================================================================

# --- PHASE 1: INITIALIZATION ---
add, $r2, $zero, $zero, 0      # Shared Counter Address
add, $r3, $zero, $imm, 3       # My Core ID = 3
add, $r4, $zero, $imm, 512     # Maximum count
add, $r5, $zero, $imm, 3       # Mask for modulo 4
add, $r7, $zero, $imm, 512     # Sync address

# --- PHASE 2: SPIN-LOCK ---
lw, $r6, $r2, $zero, 0         # [PC 5] Load shared counter
beq, $imm, $r6, $r4, 12        # [PC 6] Exit if target reached

# Turn check: (Counter & 3) == 3?
and, $r8, $r6, $r5, 0          # [PC 7] Calculate turn index
bne, $imm, $r8, $r3, 5         # [PC 8] If turn is not 3, spin back to PC 5

# --- PHASE 3: CRITICAL SECTION ---
add, $r6, $r6, $imm, 1         # [PC 9] Increment
sw, $r6, $r2, $zero, 0         # [PC 10] Store result
beq, $imm, $zero, $zero, 5     # [PC 11] Jump to loop start

# --- PHASE 4: TERMINATION ---
lw, $r8, $r7, $zero, 0         # Final sync read
halt, $zero, $zero, $zero, 0   # Stop Core 3