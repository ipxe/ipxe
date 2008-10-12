/* -*- sh -*- */

/*
 * Linker script for EFI images
 *
 */

EXTERN ( efi_entry )

SECTIONS {

    /* The file starts at a virtual address of zero, and sections are
     * contiguous.  Each section is aligned to at least _max_align,
     * which defaults to 32.  Load addresses are equal to virtual
     * addresses.
     */

    . = 0;
    PROVIDE ( _max_align = 32 );

    /*
     * The prefix
     *
     */

    .prefix : AT ( _prefix_lma ) {
	_prefix = .;
	*(.prefix)
	*(.prefix.*)
	_mprefix = .;
    } .prefix_bss (NOLOAD) : {
	_eprefix = .;
    }
    _prefix_filesz	= ABSOLUTE ( _mprefix - _prefix );
    _prefix_memsz	= ABSOLUTE ( _eprefix - _prefix );

    /*
     * The text section
     *
     */

    . = ALIGN ( _max_align );
    .text : AT ( _text_lma ) {
	_text = .;
	*(.text)
	*(.text.*)
	_mtext = .;
    } .text_bss (NOLOAD) : {
	_etext = .;
    }
    _text_filesz	= ABSOLUTE ( _mtext - _text );
    _text_memsz		= ABSOLUTE ( _etext - _text );

    /*
     * The rodata section
     *
     */

    . = ALIGN ( _max_align );
    .rodata : AT ( _rodata_lma ) {
	_rodata = .;
	*(.rodata)
	*(.rodata.*)
	_mrodata = .;
    } .rodata_bss (NOLOAD) : {
	_erodata = .;
    }
    _rodata_filesz	= ABSOLUTE ( _mrodata - _rodata );
    _rodata_memsz	= ABSOLUTE ( _erodata - _rodata );

    /*
     * The data section
     *
     */

    . = ALIGN ( _max_align );
    .data : AT ( _data_lma ) {
	_data = .;
	*(.data)
	*(.data.*)
	*(SORT(.tbl.*))		/* Various tables.  See include/tables.h */
	/* EFI seems to not support proper bss sections */
	*(.bss)
	*(.bss.*)
	*(COMMON)
	*(.stack)
	*(.stack.*)
	_mdata = .;
    } .data_bss (NOLOAD) : {
	_edata = .;
    }
    _data_filesz	= ABSOLUTE ( _mdata - _data );
    _data_memsz		= ABSOLUTE ( _edata - _data );

    /*
     * The bss section
     *
     */

    . = ALIGN ( _max_align );
    .bss : AT ( _bss_lma ) {
	_bss = .;
	/* EFI seems to not support proper bss sections */
	_mbss = .;
    } .bss_bss (NOLOAD) : {
	_ebss = .;
    }
    _bss_filesz		= ABSOLUTE ( _mbss - _bss );
    _bss_memsz		= ABSOLUTE ( _ebss - _bss );

    /*
     * The reloc section
     *
     */

    . = ALIGN ( _max_align );
    .reloc : AT ( _reloc_lma ) {
	_reloc = .;
	/* Provide some dummy contents to force ld to include this
	 * section.  It will be created by the efilink utility.
	 */
	. += 1;
	_mreloc = .;
    } .reloc_bss (NOLOAD) : {
	_ereloc = .;
    }
    _reloc_filesz	= ABSOLUTE ( _mreloc - _reloc );
    _reloc_memsz	= ABSOLUTE ( _ereloc - _reloc );

    _filesz		= ABSOLUTE ( . );

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
	*(.note)
    }

    /*
     * Load address calculations.
     *
     */

    _prefix_lma		= ABSOLUTE ( _prefix );
    _text_lma		= ABSOLUTE ( _text );
    _rodata_lma		= ABSOLUTE ( _rodata );
    _data_lma		= ABSOLUTE ( _data );
    _bss_lma		= ABSOLUTE ( _bss );
    _reloc_lma		= ABSOLUTE ( _reloc );

    /*
     * Load addresses required by the prefix
     *
     */
    efi_entry_lma	= ABSOLUTE ( efi_entry );
    debugdir_lma	= ABSOLUTE ( debugdir );
    codeview_rsds_lma	= ABSOLUTE ( codeview_rsds );
}
