# Counter Program - Core X
# $r2 = Counter Address (0)
# $r3 = Turn Address (1)
# $r4 = Loop Counter (128)
# $r5 = My ID (0, 1, 2, or 3)
# $r6 = Next ID (1, 2, 3, or 0)
# $r7 = Temporary for Data
# $r8 = Temporary for Reading Turn

# --- Initialization ---
    add $r2, $zero, $imm, 0       # r2 = 0 (Counter Addr)
    add $r3, $zero, $imm, 1       # r3 = 1 (Turn Addr)
    add $r4, $zero, $imm, 128     # r4 = 128 (Loop Count)

# !!! CHANGE THESE TWO LINES FOR EACH CORE !!!
    add $r5, $zero, $imm, 0       # r5 = My ID (Set to 0, 1, 2, or 3)
    add $r6, $zero, $imm, 1       # r6 = Next ID (Set to 1, 2, 3, or 0)

Loop:
    beq $r4, $zero, $imm, Done    # If r4 == 0, finish

WaitTurn:
    lw $r8, $r3, $imm, 0          # Read Turn Variable (Addr 1)
    bne $r8, $r5, $imm, WaitTurn  # If (Turn != MyID), keep waiting

CriticalSection:
    lw $r7, $r2, $imm, 0          # Read Counter
    add $r7, $r7, $imm, 1         # Increment
    sw $r7, $r2, $imm, 0          # Write Counter

PassTurn:
    sw $r6, $r3, $imm, 0          # Write Next ID to Turn Variable

    sub $r4, $r4, $imm, 1         # Decrement Loop Counter
    beq $zero, $zero, $imm, Loop  # Jump back to Loop

Done:
    halt $zero, $zero, $zero, 0