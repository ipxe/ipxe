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
     * The linker symbols {prefix,decompress,text,data}_link_addr,
     * load_addr, and _max_align may be specified explicitly.  If not
     * specified, they will default to:
     *
     *   _prefix_link_addr	= 0
     *   _decompress_link_addr	= 0
     *   _text_link_addr	= 0
     *   _data_link_addr	= _text_link_addr + sizeof ( text sections )
     *   _load_addr		= 0
     *   _max_align		= 16
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
     * The real-mode prefixes rely on _text_link_addr and
     * _decompress_link_addr being 0, since they issue far calls into
     * those sections, thus requiring that symbol_value ==
     * symbol_offset therein.  Using the linker to calculate
     * e.g. offset_setup16=setup16-_text will not work, since you then
     * cannot use the reference from the prefix to setup16 to drag in
     * setup16.o.  Life is hard.
     *
     * If librm is included, then it must go at offset 0 within the
     * text section.  This is because librm is dual-usage: it is
     * called from setup16 with %cs:0000 pointing to the start of the
     * text section, and later it will be copied to base memory and
     * called with %cs:0000 pointing to the start of librm.
     *
     * The decompressor is designed to decompress in-place.  After
     * calling the decompressor, the image will look exactly the same
     * as the uncompressed image; the compressed data and the
     * decompressor code itself will have been overwritten.
     */

    /*
     * The prefix
     */

    _prefix_link_addr = DEFINED ( _prefix_link_addr ) ? _prefix_link_addr : 0;
    . = _prefix_link_addr;
    _prefix = .;

    .prefix : AT ( _prefix_load_offset + __prefix ) {
	__prefix = .;
	_entry = .;
	*(.prefix)
	*(.prefix.*)
    }
    
    _eprefix = .;

    /*
     * The decompressor (may be absent)
     */

    _decompress_link_addr = DEFINED ( _decompress_link_addr ) ?
			      _decompress_link_addr : 0;
    . = _decompress_link_addr;
    _decompress = .;

    .decompress : AT ( _decompress_load_offset + __decompress ) {
	    __decompress = .;
	    *(.decompress)
	    *(.decompress.*)
    }

    _edecompress = .;

    /*
     * The text sections
     */

    _text_link_addr = DEFINED ( _text_link_addr ) ? _text_link_addr : 0;
    . = _text_link_addr;
    _text = .;

    .text16 : AT ( _text_load_offset + __text16 ) {
	__text16 = .;

	/* librm is a special case; it must go at the start of the
	 * text section if it is included.
	 */
	_assert = ASSERT ( ( . == _text_link_addr ), "librm cannot go first" );
	*(.librm)

	*(.text16)
	*(.text16.*)
    } = 0x9090

    .text : AT ( _text_load_offset + __text ) {
	__text = .;
	*(.text)
	*(.text.*)
    } = 0x9090

    _etext = .;

    /*
     * The data sections
     */

    _data_link_addr = DEFINED ( _data_link_addr ) ? _data_link_addr : .;
    . = _data_link_addr;
    _data = .;

    .rodata : AT ( _data_load_offset + __rodata ) {
	__rodata = .;
	*(.rodata)
	*(.rodata.*)
    }

    .data : AT ( _data_load_offset + __data ) {
	__data = .;
	*(.data)
	*(.data.*)

	/* Various tables.  See include/tables.h for an explanation. */
	*(SORT(.tbl.*))	


	isa_drivers = . ;
	*(.drivers.isa)
	isa_drivers_end = .;

	_progbits_end = .;
    }

    .bss : AT ( _data_load_offset + __bss ) {
	__bss = .;
	_bss = .;
	*(.bss)
	*(.bss.*)
	*(COMMON)
	_ebss = .;
    }

    .stack : AT ( _data_load_offset + __stack ) {
	__stack = .;
	*(.stack)
	*(.stack.*)
    }

    _edata = .;

    _end = .;

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

    _max_align		    = DEFINED ( _max_align ) ? _max_align : 16;
    _load_addr		    = DEFINED ( _load_addr ) ? _load_addr : 0;

    .			    = _load_addr;

    .			   -= _prefix_link_addr;
    _prefix_load_offset	    = ALIGN ( _max_align );
    _prefix_load_addr	    = _prefix_link_addr + _prefix_load_offset;
    _prefix_size	    = _eprefix - _prefix;
    .			    = _prefix_load_addr + _prefix_size;

    .			   -= _decompress_link_addr;
    _decompress_load_offset = ALIGN ( _max_align );
    _decompress_load_addr   = _decompress_link_addr + _decompress_load_offset;
    _decompress_size	    = _edecompress - _decompress;
    .			    = _decompress_load_addr + _decompress_size;

    .			   -= _text_link_addr;
    _text_load_offset	    = ALIGN ( _max_align );
    _text_load_addr	    = _text_link_addr + _text_load_offset;
    _text_size		    = _etext - _text;
    .			    = _text_load_addr + _text_size;

    .			   -= _data_link_addr;
    _data_load_offset	    = ALIGN ( _max_align );
    _data_load_addr	    = _data_link_addr + _data_load_offset;
    _data_size		    = _edata - _data;
    .			    = _data_load_addr + _data_size;

    /*
     * Alignment checks.  ALIGN() can only operate on the location
     * counter, so we set the location counter to each value we want
     * to check.
     */

    . = _prefix_load_addr - _prefix_link_addr;
    _assert = ASSERT ( ( . == ALIGN ( _max_align ) ),
		       "_prefix is badly aligned" );

    . = _decompress_load_addr - _prefix_link_addr;
    _assert = ASSERT ( ( . == ALIGN ( _max_align ) ),
		       "_decompress is badly aligned" );

    . = _text_load_addr - _text_link_addr;
    _assert = ASSERT ( ( . == ALIGN ( _max_align ) ),
		       "_text is badly aligned" );

    . = _data_load_addr - _data_link_addr;
    _assert = ASSERT ( ( . == ALIGN ( _max_align ) ),
		       "_data is badly aligned" );

    /*
     * setup16 needs to know this when KEEP_IT_REAL is used.  There
     * are no harmful side-effects of calculating it all the time.
     */
    _text_load_size_pgh = ( _data_load_addr - _text_load_addr ) / 16 ;

    /*
     * Useful-to-know values.
     */

    /* Size of the decompressed runtime image */
    _runtime_size = _edata - _text;
    /* Size of the initialised-contents portion of the runtime image */
    _runtime_progbits_size = _progbits_end - _text;
    /* Size of the (non-compressed) binary file */
    _file_size = _prefix_size + _runtime_progbits_size;
    /* Size of the non-compressed portion of the compressed binary file */
    _zfile_noncompressed_size = _prefix_size + _decompress_size;
}
