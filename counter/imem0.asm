# ==============================================================================
# Project: Multi-Core Cache Simulator (MIPS-like)
# File:    counter/imem0.asm
# Author:
# ID:
# Date:    11/11/2024
#
# Description:
# Core 0 implementation of a shared counter update using a token-passing mechanism.
# The core waits for its turn (token), increments a shared counter, passes the
# token to the next core, and repeats.
# ==============================================================================

# ------------------------------------------------------------------------------
# REGISTER MAP
# ------------------------------------------------------------------------------
# $r2:  Address of Shared Counter (0x0)
# $r3:  Address of Token / Turn Variable (0x1)
# $r4:  My Expected Turn Value (0 for Core 0) - UNIQUE PER CORE
# $r5:  Number of Cores (4) - Used to calculate next turn
# $r6:  Loop Counter (Local iteration count)
# $r7:  Max Iterations (128)
# $r8:  Temporary register for Shared Counter value
# $r9:  Temporary register for Token value
# $r10: Address for final synchronization/signal (512)
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# INITIALIZATION
# ------------------------------------------------------------------------------
add $r2, $zero, $zero, 0        # R2 = 0 (Shared Counter Address)
add $r3, $zero, $imm, 1         # R3 = 1 (Token Address)
add $r4, $zero, $imm, 0         # R4 = 0 (My Turn: Core 0 goes on 0, 4, 8...)
add $r5, $zero, $imm, 4         # R5 = 4 (Increment step for turn)
add $r6, $zero, $zero, 0        # R6 = 0 (Iteration count)
add $r7, $zero, $imm, 128       # R7 = 128 (Total iterations)
add $r10, $zero, $imm, 512      # R10 = 512 (Sync address)

# ------------------------------------------------------------------------------
# MAIN LOOP
# ------------------------------------------------------------------------------
Loop_Start:
	# Check if we have completed all iterations
	beq $imm, $r6, $r7, End_Seq
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

# ------------------------------------------------------------------------------
# SPIN WAIT (Token Check)
# ------------------------------------------------------------------------------
Spin_Wait:
	# Read the current token value from memory
	lw $r9, $r3, $imm, 0

	# Check if it's my turn (Token == My Expected Turn)
	beq $imm, $r9, $r4, Critical
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

	# If not my turn, spin again
	beq $imm, $zero, $zero, Spin_Wait
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

# ------------------------------------------------------------------------------
# CRITICAL SECTION (Update Shared Counter)
# ------------------------------------------------------------------------------
Critical:
	# Load shared counter
	lw $r8, $r2, $imm, 0

	# Increment shared counter
	add $r8, $r8, $imm, 1

	# Store shared counter back
	sw $r8, $r2, $imm, 0

# ------------------------------------------------------------------------------
# PASS TOKEN
# ------------------------------------------------------------------------------
	# Load current token (we know it's ours, but good practice to reload or reuse R9)
	lw $r9, $r3, $imm, 0

	# Increment token to pass to next core (0 -> 1 -> 2 -> 3 -> 4...)
	add $r9, $r9, $imm, 1

	# Write new token to memory
	sw $r9, $r3, $imm, 0

# ------------------------------------------------------------------------------
# UPDATE LOCAL STATE
# ------------------------------------------------------------------------------
	# Update my expected turn for next time (MyTurn += 4)
	add $r4, $r4, $r5, 0

	# Increment loop counter
	add $r6, $r6, $imm, 1

	# Jump back to start of loop
	beq $imm, $zero, $zero, Loop_Start
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

# ------------------------------------------------------------------------------
# TERMINATION
# ------------------------------------------------------------------------------
End_Seq:
	# Final read to ensure memory consistency or signal completion
	lw $zero, $r10, $imm, 0

	halt $zero, $zero, $zero, 0     # Stop
	halt $zero, $zero, $zero, 0     # Stop
	halt $zero, $zero, $zero, 0     # Stop
	halt $zero, $zero, $zero, 0     # Stop
	halt $zero, $zero, $zero, 0     # Stop
