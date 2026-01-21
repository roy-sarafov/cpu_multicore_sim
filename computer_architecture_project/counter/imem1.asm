# ==============================================================================
# Project: Multi-Core Cache Simulator (MIPS-like)
# File:    imem1.asm
#
# Description:
# Implementation for Core 1 of a shared atomic counter.
# Coordinates via (Counter % 4 == 1) to ensure mutual exclusion.
# ==============================================================================

# --- PHASE 1: INITIALIZATION ---
add, $r2, $zero, $zero, 0      # Set R2 to Shared Counter Address (0)
add, $r3, $zero, $imm, 1       # Set R3 to 1 (This Core's ID)
add, $r4, $zero, $imm, 512     # Set R4 to Target Count (512)
add, $r5, $zero, $imm, 3       # Set R5 to Modulo Mask (3)
add, $r7, $zero, $imm, 512     # Set R7 to Sync Address (512)

# --- PHASE 2: SPIN-LOCK ---
lw, $r6, $r2, $zero, 0         # [PC 5] Fetch shared counter from memory
beq, $imm, $r6, $r4, 12        # [PC 6] Check if simulation is complete (Total = 512)

# Verify turn: Does (Counter & 3) match CoreID 1?
and, $r8, $r6, $r5, 0          # [PC 7] Isolate last two bits of counter
bne, $imm, $r8, $r3, 5         # [PC 8] If turn index != 1, spin (back to PC 5)

# --- PHASE 3: CRITICAL SECTION ---
add, $r6, $r6, $imm, 1         # [PC 9] Increment counter
sw, $r6, $r2, $zero, 0         # [PC 10] Commit update to shared memory
beq, $imm, $zero, $zero, 5     # [PC 11] Repeat loop

# --- PHASE 4: TERMINATION ---
lw, $r8, $r7, $zero, 0         # [PC 12] Final sync memory access
halt, $zero, $zero, $zero, 0   # Stop Core 1 execution