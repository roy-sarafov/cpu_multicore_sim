# ==============================================================================
# Project: Multi-Core Cache Simulator (MIPS-like)
# File:    imem0.asm
#
# Description:
# Implementation for Core 0 of a shared atomic counter. 
# This program uses a modulo-based spin-lock (Counter % 4) to coordinate 
# access to a shared memory variable among four cores.
# ==============================================================================

# ------------------------------------------------------------------------------
# REGISTER ALLOCATION
# ------------------------------------------------------------------------------
# $r2: Base address for the shared counter (Memory Address 0x0)
# $r3: This core's unique identifier (Core ID = 0)
# $r4: Target termination value (512)
# $r5: Bitmask used to perform modulo 4 operation (Value 3 = 0b11)
# $r6: Local register to store the counter value retrieved from memory
# $r7: Synchronization/Barrier address (Memory Address 512)
# $r8: Temporary register for turn-calculation results
# ------------------------------------------------------------------------------

# --- PHASE 1: INITIALIZATION ---
# Setup pointers and constants required for the synchronization loop.
add, $r2, $zero, $zero, 0       # Initialize R2 with 0 (Shared Counter Address)
add, $r3, $zero, $imm, 0        # Set R3 to 0 (This Core's ID)
add, $r4, $zero, $imm, 512      # Set R4 to 512 (Upper bound for counting)
add, $r5, $zero, $imm, 3        # Set R5 to 3 (Mask used for 'Counter % 4' logic)
add, $r7, $zero, $imm, 512      # Set R7 to 512 (Memory address for final sync)

# --- PHASE 2: SPIN-LOCK & TURN VALIDATION ---
# Core 0 enters a loop, continuously checking if it is its turn to increment.
lw, $r6, $r2, $zero, 0          # [PC 5] Load current shared counter from address 0x0

# Check if global goal is reached.
beq, $imm, $r6, $r4, 12         # [PC 6] If counter == 512, exit to termination (PC 12)

# Calculate turn: (Counter & 3) == MyCoreID?
and, $r8, $r6, $r5, 0           # [PC 7] R8 = R6 & 0x3 (Calculates current turn index)
bne, $imm, $r8, $r3, 5          # [PC 8] If turn index != 0, repeat load (Spin-lock)

# --- PHASE 3: CRITICAL SECTION ---
# Only reached when (Counter % 4) == 0.
add, $r6, $r6, $imm, 1          # [PC 9] Increment the local copy of the counter
sw, $r6, $r2, $zero, 0          # [PC 10] Store updated value back to shared memory

# Jump back to re-evaluate the loop.
beq, $imm, $zero, $zero, 5      # [PC 11] Unconditional jump back to Load (PC 5)

# --- PHASE 4: TERMINATION ---
lw, $r8, $r7, $zero, 0          # [PC 12] Final dummy read from sync address 512
halt, $zero, $zero, $zero, 0    # [PC 13] Stop Core 0 execution