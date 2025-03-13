	.globl getch
getch:
	addi sp, sp, -16
	sw ra, 12(sp)
	sw s0, 0(sp)
	sw s1, 4(sp)
getch_check:
	# non-blocking read
	li s0, 0x10000000
	lb s1, 5(s0)
	andi s1, s1, 0x001
	bnez s1, getch_get
getch_null:
	mv a0, x0
	j getch_epilogue
getch_get:
	lb a0, 0(s0)
	andi a0, a0, 0x0ff
getch_epilogue:
	lw s0, 0(sp)
	lw s1, 4(sp)
	lw ra, 12(sp)
	addi sp, sp, 16
	ret
