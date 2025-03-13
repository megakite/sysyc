	.globl putch
putch:
	addi sp, sp, -16
	sw ra, 12(sp)
	sw s0, 0(sp)
	sw s1, 4(sp)
putch_poll:
	li s0, 0x10000000
	lb s1, 5(s0)
	andi s1, s1, 0x020
	beqz s1, putch_poll
putch_put:
	sb a0, 0(s0)
	lw s0, 0(sp)
	lw s1, 4(sp)
	lw ra, 12(sp)
	addi sp, sp, 16
	ret
