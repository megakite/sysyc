	.globl usleep
usleep:
	addi sp, sp, -16
	sw ra, 12(sp)
	sw s0, 0(sp)
	sw s1, 4(sp)
usleep_start:
	li s0, 10
	mul a0, a0, s0
	csrr s0, time
usleep_loop:
	csrr s1, time
	sub s1, s1, s0
	blt s1, a0, usleep_loop
usleep_end:
	lw s0, 0(sp)
	lw s1, 4(sp)
	lw ra, 12(sp)
	addi sp, sp, 16
	ret
