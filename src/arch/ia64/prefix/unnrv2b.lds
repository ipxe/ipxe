OUTPUT_FORMAT("elf64-ia64-little")

OUTPUT_ARCH(ia64)

ENTRY(_start)
SECTIONS {
	. = 0;
	__gp = .;
	_text = . ;
	.text : {
		*(.text)
	}
	/DISCARD/ : {
		*(.comment)
		*(.note)
		*(.hash)
		*(.data)
		*(.sbss)
		*(.bss)
		*(.dynstr)
		*(.dynsym)
		*(.IA_64.unwind)
		*(.IA_64.unwind_info)
		*(.IA64_unwind)
		*(.IA64_unwind_info)
		*(.dynamic)
	}
}
