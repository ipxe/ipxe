/* -*- sh -*- */

/*
 * Linker script for EFI images
 *
 */

SECTIONS {

    /* The file starts at a virtual address of zero, and sections are
     * contiguous.  Each section is aligned to at least _page_align,
     * which defaults to 4096.  Load addresses are equal to virtual
     * addresses.
     */

    _page_align = 4096;

    /* Allow one page of space for file headers, common PE/COFF layout */
    . = _page_align;

    /*
     * The text section
     *
     */

    . = ALIGN ( _page_align );
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

    . = ALIGN ( _page_align );
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
     */

    . = ALIGN ( _page_align );
    .data : {
	_data = .;
	*(.sdata)
	*(.sdata.*)
	*(.data)
	*(.data.*)
	KEEP(*(SORT(.tbl.*)))	/* Various tables.  See include/tables.h */
	KEEP(*(.provided))
	KEEP(*(.provided.*))
	_edata = .;
    }

    /*
     * The bss section
     *
     */

    . = ALIGN ( _page_align );
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
     * The SBAT section
     *
     */

    . = ALIGN ( _page_align );
    .sbat : {
	_sbat = .;
	KEEP(*(.sbat))
	KEEP(*(.sbat.*))
	_esbat = .;
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
	*(.einfo)
	*(.einfo.*)
	*(.discard)
	*(.discard.*)
	*(.pci_devlist.*)
    }
}
