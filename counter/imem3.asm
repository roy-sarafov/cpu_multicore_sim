# --- INITIALIZATION ---
add $r2, $zero, $zero, 0        # R2 = 0
add $r3, $zero, $imm, 1         # R3 = 1
add $r4, $zero, $imm, 3         # R4 = 3 (My Expected Turn - UNIQUE TO CORE 3)
add $r5, $zero, $imm, 4         # R5 = 4
add $r6, $zero, $zero, 0        # R6 = 0
add $r7, $zero, $imm, 128       # R7 = 128
add $r10, $zero, $imm, 512      # R10 = 512

# --- MAIN LOOP ---
Loop_Start:
beq $imm, $r6, $r7, End_Seq
add $zero, $zero, $zero, 0      # [DELAY SLOT]

# --- SPIN WAIT ---
Spin_Wait:
lw $r9, $r3, $imm, 0
beq $imm, $r9, $r4, Critical
add $zero, $zero, $zero, 0      # [DELAY SLOT]
beq $imm, $zero, $zero, Spin_Wait
add $zero, $zero, $zero, 0      # [DELAY SLOT]

# --- CRITICAL SECTION ---
Critical:
lw $r8, $r2, $imm, 0
add $r8, $r8, $imm, 1
sw $r8, $r2, $imm, 0

# --- PASS TOKEN ---
lw $r9, $r3, $imm, 0
add $r9, $r9, $imm, 1
sw $r9, $r3, $imm, 0

# --- UPDATE LOCAL STATE ---
add $r4, $r4, $r5, 0
add $r6, $r6, $imm, 1
beq $imm, $zero, $zero, Loop_Start
add $zero, $zero, $zero, 0      # [DELAY SLOT]

# --- FINALIZE ---
End_Seq:
lw $zero, $r10, $imm, 0
halt $zero, $zero, $zero, 0     # Stop
halt $zero, $zero, $zero, 0     # Stop
halt $zero, $zero, $zero, 0     # Stop
halt $zero, $zero, $zero, 0     # Stop
halt $zero, $zero, $zero, 0     # Stop