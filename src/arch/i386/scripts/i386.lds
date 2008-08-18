/* -*- sh -*- */

/*
 * Linker script for i386 images
 *
 */

OUTPUT_FORMAT ( "elf32-i386", "elf32-i386", "elf32-i386" )
OUTPUT_ARCH ( i386 )
ENTRY ( _entry )

SECTIONS {

    /* All sections in the resulting file have consecutive load
     * addresses, but may have individual link addresses depending on
     * the memory model being used.
     *
     * We guarantee alignment of virtual addresses to any alignment
     * specified by the constituent object files (e.g. via
     * __attribute__((aligned(x)))).  Load addresses are guaranteed
     * only up to _max_align.  Provided that all loader and relocation
     * code honours _max_align, this means that physical addresses are
     * also guaranteed up to _max_align.
     *
     * Note that when using -DKEEP_IT_REAL, the UNDI segments are only
     * guaranteed to be loaded on a paragraph boundary (i.e. 16-byte
     * alignment).  Using _max_align>16 will therefore not guarantee
     * >16-byte alignment of physical addresses when -DKEEP_IT_REAL is
     * used (though virtual addresses will still be fully aligned).
     *
     */

    /*
     * The prefix
     */

    _prefix_link_addr = 0;
    . = _prefix_link_addr;
    _prefix = .;

    .prefix : AT ( _prefix_load_offset + __prefix ) {
	__prefix = .;
	_entry = .;
	*(.prefix)
	*(.prefix.*)
	_eprefix_progbits = .;
    }
    
    _eprefix = .;

    /*
     * The 16-bit sections, if present
     */

    _text16_link_addr = 0;
    . = _text16_link_addr;
    _text16 = .;

    . += 1;			/* Prevent NULL being valid */

    .text16 : AT ( _text16_load_offset + __text16 ) {
	__text16 = .;
	*(.text16)
	*(.text16.*)
	_etext16_progbits = .;
    } = 0x9090

    _etext16 = .;

    _data16_link_addr = 0;
    . = _data16_link_addr;
    _data16 = .;

    . += 1;			/* Prevent NULL being valid */

    .rodata16 : AT ( _data16_load_offset + __rodata16 ) {
	__rodata16 = .;
	*(.rodata16)
	*(.rodata16.*)
    }
    .data16 : AT ( _data16_load_offset + __data16 ) {
	__data16 = .;
	*(.data16)
	*(.data16.*)
	*(SORT(.tbl.data16.*))	/* Various tables.  See include/tables.h */
	_edata16_progbits = .;
    }
    .bss16 : AT ( _data16_load_offset + __bss16 ) {
	__bss16 = .;
	_bss16 = .;
	*(.bss16)
	*(.bss16.*)
	_ebss16 = .;
    }
    .stack16 : AT ( _data16_load_offset + __stack16 ) {
	__stack16 = .;
	*(.stack16)
	*(.stack16.*)
    }

    _edata16 = .;

    /*
     * The 32-bit sections
     */

    _textdata_link_addr = 0;
    . = _textdata_link_addr;
    _textdata = .;

    _text = .;

    . += 1;			/* Prevent NULL being valid */

    .text : AT ( _textdata_load_offset + __text ) {
	__text = .;
	*(.text.null_trap)
	*(.text)
	*(.text.*)
    } = 0x9090

    _etext = .;

    _data = .;

    .rodata : AT ( _textdata_load_offset + __rodata ) {
	__rodata = .;
	*(.rodata)
	*(.rodata.*)
    }
    .data : AT ( _textdata_load_offset + __data ) {
	__data = .;
	*(.data)
	*(.data.*)
	*(SORT(.tbl.*))		/* Various tables.  See include/tables.h */
	_etextdata_progbits = .;
    }
    .bss : AT ( _textdata_load_offset + __bss ) {
	__bss = .;
	_bss = .;
	*(.bss)
	*(.bss.*)
	*(COMMON)
	_ebss = .;
    }
    .stack : AT ( _textdata_load_offset + __stack ) {
	__stack = .;
	*(.stack)
	*(.stack.*)
    }

    _edata = .;

    _etextdata = .;

    _end = .;

    /*
     * Compressor information block
     */

    _zinfo_link_addr = 0;
    . = _zinfo_link_addr;
    _zinfo = .;

    .zinfo : AT ( _zinfo_load_offset + __zinfo ) {
	__zinfo = .;
	_entry = .;
	*(.zinfo)
	*(.zinfo.*)
	_ezinfo_progbits = .;
    }
    
    _ezinfo = .;

    /*
     * Dispose of the comment and note sections to make the link map
     * easier to read
     */

    /DISCARD/ : {
	*(.comment)
	*(.note)
    }

    /*
     * Load address calculations.  The slightly obscure nature of the
     * calculations is because ALIGN(x) can only operate on the
     * location counter.
     */

    _max_align		    = 16;
    _load_addr		    = 0;

    .			    = _load_addr;

    .			   -= _prefix_link_addr;
    _prefix_load_offset	    = ALIGN ( _max_align );
    _prefix_load_addr	    = _prefix_link_addr + _prefix_load_offset;
    _prefix_size	    = _eprefix - _prefix;
    _prefix_progbits_size   = _eprefix_progbits - _prefix;
    .			    = _prefix_load_addr + _prefix_progbits_size;

    .			   -= _text16_link_addr;
    _text16_load_offset	    = ALIGN ( _max_align );
    _text16_load_addr	    = _text16_link_addr + _text16_load_offset;
    _text16_size	    = _etext16 - _text16;
    _text16_progbits_size   = _etext16_progbits - _text16;
    .			    = _text16_load_addr + _text16_progbits_size;

    .			   -= _data16_link_addr;
    _data16_load_offset	    = ALIGN ( _max_align );
    _data16_load_addr	    = _data16_link_addr + _data16_load_offset;
    _data16_size	    = _edata16 - _data16;
    _data16_progbits_size   = _edata16_progbits - _data16;
    .			    = _data16_load_addr + _data16_progbits_size;

    .			   -= _textdata_link_addr;
    _textdata_load_offset   = ALIGN ( _max_align );
    _textdata_load_addr	    = _textdata_link_addr + _textdata_load_offset;
    _textdata_size	    = _etextdata - _textdata;
    _textdata_progbits_size = _etextdata_progbits - _textdata;
    .			    = _textdata_load_addr + _textdata_progbits_size;

    _load_size		    = . - _load_addr;

    .			   -= _zinfo_link_addr;
    _zinfo_load_offset	    = ALIGN ( _max_align );
    _zinfo_load_addr	    = _zinfo_link_addr + _zinfo_load_offset;
    _zinfo_size		    = _ezinfo - _zinfo;
    _zinfo_progbits_size    = _ezinfo_progbits - _zinfo;
    .			    = _zinfo_load_addr + _zinfo_progbits_size;

    _payload_offset	    = _text16_load_offset;

    /*
     * Alignment checks.  ALIGN() can only operate on the location
     * counter, so we set the location counter to each value we want
     * to check.
     */

    . = _prefix_load_addr - _prefix_link_addr;
    _assert = ASSERT ( ( . == ALIGN ( _max_align ) ),
		       "_prefix is badly aligned" );

    . = _text16_load_addr - _text16_link_addr;
    _assert = ASSERT ( ( . == ALIGN ( _max_align ) ),
		       "_text16 is badly aligned" );

    . = _data16_load_addr - _data16_link_addr;
    _assert = ASSERT ( ( . == ALIGN ( _max_align ) ),
		       "_data16 is badly aligned" );

    . = _textdata_load_addr - _textdata_link_addr;
    _assert = ASSERT ( ( . == ALIGN ( _max_align ) ),
		       "_text is badly aligned" );

    /*
     * Values calculated to save code from doing it
     */
    _prefix_size_pgh	= ( ( _prefix_size + 15 ) / 16 );
    _prefix_size_sect	= ( ( _prefix_size + 511 ) / 512 );
    _text16_size_pgh	= ( ( _text16_size + 15 ) / 16 );
    _data16_size_pgh	= ( ( _data16_size + 15 ) / 16 );

    /*
     * Load sizes in paragraphs and sectors.  Note that wherever the
     * _load_size variables are used, there must be a corresponding
     * .zinfo.fixup section.
     */
    _load_size_pgh	= ( ( _load_size + 15 ) / 16 );
    _load_size_sect	= ( ( _load_size + 511 ) / 512 );
}
