# Core 3: Rows 12-15

add $r2, $zero, $imm, 12
add $r12, $zero, $imm, 16

loop_i:
    beq $imm, $r2, $r12, flush_start
    add $r3, $zero, $zero, 0

loop_j:
    add $r14, $zero, $imm, 16
    beq $imm, $r3, $r14, next_i
    add $r4, $zero, $zero, 0
    add $r5, $zero, $zero, 0

loop_k:
    add $r14, $zero, $imm, 16
    beq $imm, $r4, $r14, store_c
    add $zero, $zero, $zero, 0

    sll $r9, $r2, $imm, 4
    add $r9, $r9, $r4, 0
    lw  $r9, $r9, $zero, 0

    sll $r10, $r4, $imm, 4
    add $r10, $r10, $r3, 0
    add $r10, $r10, $imm, 256
    lw  $r10, $r10, $zero, 0

    mul $r13, $r9, $r10, 0
    add $r5, $r5, $r13, 0

    add $r4, $r4, $imm, 1
    beq $imm, $zero, $zero, loop_k
    add $zero, $zero, $zero, 0

store_c:
    sll $r8, $r2, $imm, 4
    add $r8, $r8, $r3, 0
    add $r8, $r8, $imm, 512
    sw  $r5, $r8, $zero, 0

    add $r3, $r3, $imm, 1
    beq $imm, $zero, $zero, loop_j
    add $zero, $zero, $zero, 0

next_i:
    add $r2, $r2, $imm, 1
    beq $imm, $zero, $zero, loop_i
    add $zero, $zero, $zero, 0

flush_start:
    add $r2, $zero, $zero, 0
    add $r12, $zero, $imm, 256

flush_loop:
    beq $imm, $r2, $r12, halt_core
    add $zero, $zero, $zero, 0
    lw $r0, $r2, $zero, 0
    add $r2, $r2, $imm, 8
    beq $imm, $zero, $zero, flush_loop
    add $zero, $zero, $zero, 0

halt_core:
    halt $zero, $zero, $zero, 0