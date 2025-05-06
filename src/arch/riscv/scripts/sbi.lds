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
	_edata = .;
    }

    /* Uninitialised and discardable data */
    OVERLAY : {

	/* Runtime relocations (discarded after use) */
	.rela.dyn {
	    _reloc = .;
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

    /* Absolute link-time address of _reloc
     *
     * The runtime relocator needs to know the link-time addresses.
     * Since RISC-V uses .rela (rather than .rel), the only way to
     * expose this to the relocator is via an absolute symbol.
     */
    _reloc_base = ABSOLUTE ( _reloc );

    /* Calculate end of relocations
     *
     * This cannot be done by placing "_ereloc = .;" inside the
     * .rela.dyn section, since the dynamic relocations are not
     * present in the input sections but are instead generated during
     * linking.
     */
    _ereloc = ( _reloc + __load_stop_reladyn - __load_start_reladyn );

    /* Length of initialised data */
    _sbi_filesz = ( ABSOLUTE ( _ereloc ) - ABSOLUTE ( _prefix ) );

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
