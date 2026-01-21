# ==============================================================================
# File:    imem[1-3].asm
# Description:
# Idle program for secondary cores. These cores perform no operations 
# and halt immediately to allow Core 0 to perform serial matrix multiplication.
# ==============================================================================

halt $zero, $zero, $zero, 0