/* -*- sh -*- */

/*
 * Linker script for Linux images
 *
 */

SECTIONS {
	_max_align = 32;

	/*
	 * The text section
	 *
	 */

	. = ALIGN ( _max_align );
	.text : {
		_text = .;
		*(.text)
		*(.text.*)
		_etext = .;
	}

	/*
	 * The rodata section
	 *
	 */

	. = ALIGN ( _max_align );
	.rodata : {
		_rodata = .;
		*(.srodata)
		*(.srodata.*)
		*(.rodata)
		*(.rodata.*)
		_erodata = .;
	}

	/*
	 * The data section
	 *
	 * Adjust the address for the data segment.  We want to adjust up to
	 * the same address within the page on the next page up.
	 */

	. = ALIGN (CONSTANT (MAXPAGESIZE)) - ((CONSTANT (MAXPAGESIZE) - .) & (CONSTANT (MAXPAGESIZE) - 1));
	. = DATA_SEGMENT_ALIGN (CONSTANT (MAXPAGESIZE), CONSTANT (COMMONPAGESIZE));
	.data : {
		_data = .;
		*(.sdata)
		*(.sdata.*)
		*(.data)
		*(.data.*)
		KEEP(*(SORT(.tbl.*)))
		KEEP(*(.provided))
		KEEP(*(.provided.*))
		_edata = .;
	}

	/*
	 * The bss section
	 *
	 */

	. = ALIGN ( _max_align );
	.bss : {
		_bss = .;
		*(.sbss)
		*(.sbss.*)
		*(.bss)
		*(.bss.*)
		*(COMMON)
		_ebss = .;
	}

	/*
	 * Weak symbols that need zero values if not otherwise defined
	 *
	 */

	.weak 0x0 : {
		_weak = .;
		*(.weak)
		*(.weak.*)
		_eweak = .;
	}
	_assert = ASSERT ( ( _weak == _eweak ), ".weak is non-zero length" );

	/*
	 * Dispose of unwanted sections to make the link map easier to read
	 *
	 */

	/DISCARD/ : {
		*(.comment)
		*(.comment.*)
		*(.rel)
		*(.rel.*)
		*(.discard)
		*(.discard.*)
		*(.sbat)
		*(.sbat.*)
	}
}
