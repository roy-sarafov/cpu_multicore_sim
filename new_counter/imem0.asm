# ==============================================================================
# Project: Multi-Core Cache Simulator (MIPS-like)
# File:    new_counter/imem0.asm
# Author:
# ID:
# Date:    11/11/2024
#
# Description:
# Core 0 implementation of a shared counter update.
# This version uses a simpler modulo-based check to determine turn.
# ==============================================================================

# ------------------------------------------------------------------------------
# REGISTER MAP
# ------------------------------------------------------------------------------
# $r2: Address of Shared Counter (0x0)
# $r3: My Core ID (0)
# $r4: Max Count (512)
# $r5: Mask for Modulo 4 (3 -> 0b11)
# $r6: Current Counter Value (Loaded from memory)
# $r7: Sync Address (512)
# $r8: Temporary result of (Counter % 4)
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# INITIALIZATION
# ------------------------------------------------------------------------------
add, $r2, $zero, $zero, 0       # R2 = 0 (Shared Counter Address)
add, $r3, $zero, $imm, 0        # R3 = 0 (My Core ID)
add, $r4, $zero, $imm, 512      # R4 = 512 (Target Count)
add, $r5, $zero, $imm, 3        # R5 = 3 (Mask for Modulo 4)
add, $r7, $zero, $imm, 512      # R7 = 512 (Sync Address)

# ------------------------------------------------------------------------------
# MAIN LOOP
# ------------------------------------------------------------------------------
# PC=5: Load current counter value
lw, $r6, $r2, $zero, 0

# PC=6: Check if counter reached max (512)
beq, $imm, $r6, $r4, 12         # If R6 == 512, jump to End (PC=12)

# PC=7: Check if it's my turn (Counter % 4 == CoreID)
and, $r8, $r6, $r5, 0           # R8 = R6 & 3 (Equivalent to R6 % 4)
bne, $imm, $r8, $r3, 5          # If (R8 != MyID), jump back to Load (PC=5)

# ------------------------------------------------------------------------------
# CRITICAL SECTION
# ------------------------------------------------------------------------------
# PC=9: Increment Counter
add, $r6, $r6, $imm, 1

# PC=10: Store Counter
sw, $r6, $r2, $zero, 0

# PC=11: Jump back to start
beq, $imm, $zero, $zero, 5      # Unconditional jump to PC=5

# ------------------------------------------------------------------------------
# TERMINATION
# ------------------------------------------------------------------------------
# PC=12: Final Sync Read
lw, $r8, $r7, $zero, 0

# PC=13: Halt
halt, $zero, $zero, $zero, 0
