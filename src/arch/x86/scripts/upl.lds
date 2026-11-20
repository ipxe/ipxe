/* -*- ld-script -*- */

/*
 * Linker script for Universal Payload (UPL) images
 *
 * The image is linked to run at a fixed physical address (defined
 * via --defsym _upl_load_addr) in 32-bit flat protected mode.  The
 * image is wrapped in a FIT image, and copies itself to its link
 * address.
 *
 */

SECTIONS {

    /* Weak symbols that need zero values if not otherwise defined */
    saved_pos = .;
    .weak 0x0 : {
	_weak = .;
	*(.weak)
	*(.weak.*)
	_eweak = .;
    }
    _assert = ASSERT ( ( _weak == _eweak ), ".weak is non-zero length" );
    _assert = ASSERT ( ( . == saved_pos ), ".weak altered current position" );

    /* Fixed load address */
    . = _upl_load_addr;

    /* Prefix code */
    .prefix : {
	_prefix = .;
	KEEP(*(.prefix))
	*(.prefix.*)
	_eprefix = .;
    }

    /* Program code */
    .text : {
	_text = .;
	*(.text)
	*(.text.*)
	_etext = .;
    }

    /* Read-only data */
    . = ALIGN ( 4096 );
    .rodata : {
	_rodata = .;
	*(.srodata)
	*(.srodata.*)
	*(.rodata)
	*(.rodata.*)
	_erodata = .;
    }

    /* Writable data */
    . = ALIGN ( 4096 );
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

    /* Uninitialised data */
    .bss : {
	_bss = .;
	*(.sbss)
	*(.sbss.*)
	*(.bss)
	*(.bss.*)
	*(COMMON)
	*(.stack)
	*(.stack.*)
	. = ALIGN ( 16 );
	_ebss = .;
    }

    /* End address */
    _end = .;

    /* Length of in-memory image */
    _memsz = ( ABSOLUTE ( _end ) - ABSOLUTE ( _prefix ) );

    /* Required alignment (the image is never relocated) */
    _max_align = 16;

    /* Unwanted sections.  Real-mode sections are discarded so that
     * any accidental dependency on BIOS code causes a link failure.
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
	*(.zinfo)
	*(.zinfo.*)
	*(.text16)
	*(.text16.*)
	*(.rodata16)
	*(.rodata16.*)
	*(.data16)
	*(.data16.*)
	*(.bss16)
	*(.bss16.*)
	*(.stack16)
	*(.stack16.*)
    }
}
