
add, $r2, $zero, $zero, 0
add, $r3, $zero, $imm, 0
add, $r4, $zero, $imm, 512
add, $r5, $zero, $imm, 3
add, $r7, $zero, $imm, 512
lw, $r6, $r2, $zero, 0
beq, $imm, $r6, $r4, 12
and, $r8, $r6, $r5, 0
bne, $imm, $r8, $r3, 5

add, $r6, $r6, $imm, 1
sw, $r6, $r2, $zero, 0
beq, $imm, $zero, $zero, 5

lw, $r8, $r7, $zero, 0

halt, $zero, $zero, $zero, 0
