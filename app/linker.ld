OUTPUT_ARCH(riscv)
ENTRY(_start)
SECTIONS
{
	. = 0x80000000;
	.text : {
		*(.text.entry),
		*(.text)
	}

	. = 0x80010000;
	.data : {
		*(.data)
	}
	. = ALIGN(4K);
	.bss : {
		*(.bss.stack),
		*(.bss)
	}
}
