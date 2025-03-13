	.section .text.entry
	.globl _start
_start:
	la sp, stack_base
	call main
halt:
	j halt

	.section .bss.stack
	.globl stack_limit
stack_limit:
	.space 4096
	.globl stack_base
stack_base:
