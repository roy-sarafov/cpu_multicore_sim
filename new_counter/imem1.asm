add, $r2, $zero, $zero, 0      # $r2 = 0 (Address of the shared counter)
add, $r3, $zero, $imm, 1       # $r3 = 1 (Core ID)
add, $r4, $zero, $imm, 512     # $r4 = 512 (Stop condition)
add, $r5, $zero, $imm, 3       # $r5 = 3 (Mask for Modulo 4: 00...0011)
add, $r7, $zero, $imm, 512     # $r7 = 512 (Address 0x200 for conflict miss)
### MAIN LOOP ###
lw, $r6, $r2, $zero, 0         # Load counter value from memory (address 0)
beq, $imm, $r6, $r4, 12        # If Counter ($r6) == 512 ($r4), jump to END (Line 12)
### SYNCHRONIZATION CHECK (Round Robin) ###
and, $r8, $r6, $r5, 0          # $r8 = Counter & 3 (Calculate Counter % 4)
bne, $imm, $r8, $r3, 5         # If ($r8 != Core ID), jump back to line 5 (Main Loop) (Busy Wait)
### UPDATE COUNTER ###
add, $r6, $r6, $imm, 1         # Increment counter: $r6 = $r6 + 1
sw, $r6, $r2, $zero, 0        # Store updated counter back to memory (address 0)
beq, $imm, $zero, $zero, 5    # Unconditional jump back to Line 5 (Main Loop)
### TERMINATION AND FLUSH ###
lw, $r8, $r7, $zero, 0        # Load from addr 512 (0x200). 
                              # Maps to Set 0 (same as addr 0) but different Tag.
                              # Forces eviction of addr 0 (Modified) to Main Memory.
halt, $zero, $zero, $zero, 0  # Stop the processor
