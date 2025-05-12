/*
 * Linker script for RISC-V SBI images
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

    /* Prefix code */
    .prefix : {
        _prefix = .;
	*(.prefix)
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

    /* Align to page size to allow linker to generate W^X segments */
    . = ALIGN ( 4096 );

    /* Read-only data */
    .rodata : {
	_rodata = .;
	*(.rodata)
	*(.rodata.*)
	_erodata = .;
    }

    /* Writable data */
    .data : {
	_data = .;
	*(.data)
	*(.data.*)
	KEEP(*(SORT(.tbl.*)))	/* Various tables.  See include/tables.h */
	KEEP(*(.provided))
	KEEP(*(.provided.*))
	*(.got)
	*(.got.plt)
	/* Ensure compressed relocations end up aligned */
	. = ALIGN ( 16 );
	_edata = .;
    }

    /* Uninitialised and discardable data */
    OVERLAY : {

	/* Runtime relocations (discarded after use) */
	.rela.dyn {
	    *(.rela)
	    *(.rela.dyn)
	}

	/* Compressor information block */
	.zinfo {
	    _zinfo = .;
	    KEEP(*(.zinfo))
	    KEEP(*(.zinfo.*))
	    _ezinfo = .;
	}

	/* Uninitialised data */
	.bss {
	    _bss = .;
	    *(.bss)
	    *(.bss.*)
	    *(COMMON)
	    *(.stack)
	    *(.stack.*)
	    /* Align to allow for easy zeroing by prefix code */
	    . = ALIGN ( 16 );
	    _ebss = .;
	}
    }

    /* End virtual address */
    _end = .;

    /* Base virtual address */
    _base = ABSOLUTE ( _prefix );

    /* Relocations */
    _reloc_offset = ( LOADADDR ( .rela.dyn ) - LOADADDR ( .prefix ) );
    _reloc_filesz = SIZEOF ( .rela.dyn );

    /* Length of initialised data */
    _filesz = ( ABSOLUTE ( _edata ) - ABSOLUTE ( _prefix ) );

    /* Length of in-memory image */
    _memsz = ( ABSOLUTE ( _end ) - ABSOLUTE ( _prefix ) );

    /* Unwanted sections */
    /DISCARD/ : {
	*(.comment)
	*(.comment.*)
	*(.note)
	*(.note.*)
	*(.eh_frame)
	*(.eh_frame.*)
	*(.dynamic)
	*(.dynsym)
	*(.dynstr)
	*(.hash)
	*(.gnu.hash)
	*(.einfo)
	*(.einfo.*)
	*(.discard)
	*(.discard.*)
	*(.pci_devlist.*)
    }
}
