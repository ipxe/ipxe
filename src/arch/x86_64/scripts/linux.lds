/* -*- sh -*- */

/*
 * Linker script for x86_64 Linux images
 *
 */

OUTPUT_FORMAT ( "elf64-x86-64", "elf64-x86-64", "elf64-x86-64" )
OUTPUT_ARCH ( i386:x86-64 )

ENTRY ( _start )

SECTIONS {
	_max_align = 32;

	. = 0x400000;

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
		*(.rodata)
		*(.rodata.*)
		_erodata = .;
	}

	/*
	 * The data section
	 *
	 */

	. = ALIGN ( _max_align );
	.data : {
		_data = .;
		*(.data)
		*(.data.*)
		*(SORT(.tbl.*))		/* Various tables.  See include/tables.h */
		_edata = .;
	}

	/*
	 * The bss section
	 *
	 */

	. = ALIGN ( _max_align );
	.bss : {
		_bss = .;
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
		_eweak = .;
	}
	_assert = ASSERT ( ( _weak == _eweak ), ".weak is non-zero length" );

	/*
	 * Dispose of the comment and note sections to make the link map
	 * easier to read
	 *
	 */

	/DISCARD/ : {
		*(.comment)
		*(.comment.*)
		*(.note)
		*(.note.*)
		*(.eh_frame)
		*(.eh_frame.*)
		*(.rel)
		*(.rel.*)
		*(.discard)
	}
}
