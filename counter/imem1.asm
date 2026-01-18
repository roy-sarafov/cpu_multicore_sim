# ==============================================================================
# Project: Multi-Core Cache Simulator (MIPS-like)
# File:    counter/imem1.asm
# Author:
# ID:
# Date:    11/11/2024
#
# Description:
# Core 1 implementation of a shared counter update using a token-passing mechanism.
# Identical logic to Core 0, but waits for Turn = 1, 5, 9...
# ==============================================================================

# ------------------------------------------------------------------------------
# REGISTER MAP
# ------------------------------------------------------------------------------
# $r2:  Address of Shared Counter (0x0)
# $r3:  Address of Token / Turn Variable (0x1)
# $r4:  My Expected Turn Value (1 for Core 1) - UNIQUE PER CORE
# $r5:  Number of Cores (4)
# $r6:  Loop Counter
# $r7:  Max Iterations (128)
# $r8:  Temporary register for Shared Counter value
# $r9:  Temporary register for Token value
# $r10: Address for final synchronization (512)
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# INITIALIZATION
# ------------------------------------------------------------------------------
add $r2, $zero, $zero, 0        # R2 = 0
add $r3, $zero, $imm, 1         # R3 = 1
add $r4, $zero, $imm, 1         # R4 = 1 (My Turn: Core 1 goes on 1, 5, 9...)
add $r5, $zero, $imm, 4         # R5 = 4
add $r6, $zero, $zero, 0        # R6 = 0
add $r7, $zero, $imm, 128       # R7 = 128
add $r10, $zero, $imm, 512      # R10 = 512

# ------------------------------------------------------------------------------
# MAIN LOOP
# ------------------------------------------------------------------------------
Loop_Start:
	beq $imm, $r6, $r7, End_Seq
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

# ------------------------------------------------------------------------------
# SPIN WAIT
# ------------------------------------------------------------------------------
Spin_Wait:
	lw $r9, $r3, $imm, 0
	beq $imm, $r9, $r4, Critical
	add $zero, $zero, $zero, 0      # [DELAY SLOT]
	beq $imm, $zero, $zero, Spin_Wait
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

# ------------------------------------------------------------------------------
# CRITICAL SECTION
# ------------------------------------------------------------------------------
Critical:
	lw $r8, $r2, $imm, 0
	add $r8, $r8, $imm, 1
	sw $r8, $r2, $imm, 0

# ------------------------------------------------------------------------------
# PASS TOKEN
# ------------------------------------------------------------------------------
	lw $r9, $r3, $imm, 0
	add $r9, $r9, $imm, 1
	sw $r9, $r3, $imm, 0

# ------------------------------------------------------------------------------
# UPDATE LOCAL STATE
# ------------------------------------------------------------------------------
	add $r4, $r4, $r5, 0
	add $r6, $r6, $imm, 1
	beq $imm, $zero, $zero, Loop_Start
	add $zero, $zero, $zero, 0      # [DELAY SLOT]

# ------------------------------------------------------------------------------
# TERMINATION
# ------------------------------------------------------------------------------
End_Seq:
	lw $zero, $r10, $imm, 0
	halt $zero, $zero, $zero, 0     # Stop
	halt $zero, $zero, $zero, 0     # Stop
	halt $zero, $zero, $zero, 0     # Stop
	halt $zero, $zero, $zero, 0     # Stop
	halt $zero, $zero, $zero, 0     # Stop
