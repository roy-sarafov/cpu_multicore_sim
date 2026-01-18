# ==============================================================================
# Project: Multi-Core Cache Simulator (MIPS-like)
# File:    counter/imem2.asm
# Author:
# ID:
# Date:    11/11/2024
#
# Description:
# Core 2 implementation of a shared counter update using a token-passing mechanism.
# Identical logic to Core 0, but waits for Turn = 2, 6, 10...
# ==============================================================================

# ------------------------------------------------------------------------------
# REGISTER MAP
# ------------------------------------------------------------------------------
# $r2:  Address of Shared Counter (0x0)
# $r3:  Address of Token / Turn Variable (0x1)
# $r4:  My Expected Turn Value (2 for Core 2) - UNIQUE PER CORE
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
add $r4, $zero, $imm, 2         # R4 = 2 (My Turn: Core 2 goes on 2, 6, 10...)
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
