OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_prefix_start)
SECTIONS {
	/* Prefix */
	.prefix : {
		_verbatim_start = . ;
		_prefix_start = . ;
		*(.prefix)
		. = ALIGN(16);
		_prefix_end = . ;
	} = 0x9090
	_prefix_size = _prefix_end - _prefix_start;

	.text.nocompress : {
		*(.prefix.udata)
	} = 0x9090

	decompress_to = . ;
	.prefix.zdata : {
		_compressed = . ;
		*(.prefix.zdata)
		_compressed_end = . ;
	}
	_compressed_size = _compressed_end - _compressed;

	. = ALIGN(16);
	_verbatim_end = . ;


	/* Size of the core of etherboot in memory */
	_base_size = _end - _text;

	/* _prefix_size is the length of the non-core etherboot prefix */
	_prefix_size = _prefix_end - _prefix_start;

	/* _verbatim_size is the actual amount that has to be copied to base memory */
	_verbatim_size = _verbatim_end - _verbatim_start;

	/* _image_size is the amount of base memory needed to run */
	_image_size = _base_size +  _prefix_size;

	/* Standard sizes rounded up to paragraphs */
	_prefix_size_pgh   = (_prefix_size + 15) / 16;
	_verbatim_size_pgh = (_verbatim_size + 15) / 16;
	_image_size_pgh    = (_image_size + 15) / 16 ;
	
	/* Standard sizes in sectors */
	_prefix_size_sct   = (_prefix_size + 511) / 512;
	_verbatim_size_sct = (_verbatim_size + 511) / 512;
	_image_size_sct    = (_image_size + 511) / 512;

	/* Symbol offsets and sizes for the exe prefix */
	_exe_hdr_size   = 32;
	_exe_size       = _verbatim_size; /* Should this be - 32 to exclude the header? */
	_exe_size_tail  = (_exe_size) % 512;
	_exe_size_pages = ((_exe_size) + 511) / 512;
	_exe_bss_size   = ((_image_size - _verbatim_size) + 15) / 16;
	_exe_ss_offset  = (_stack_offset + _prefix_size - _exe_hdr_size + 15) / 16 ;

	/* This is where we copy the compressed image before decompression.
	 * Prepare to decompress in place.  The end mark is about 8.25 bytes long,
	 * and the worst case symbol is about 16.5 bytes long.  Therefore
	 * We need to reserve at least 25 bytes of slack here.  
	 * Currently I reserve 2048 bytes of just slack to be safe :)
	 * 2048 bytes easily falls within the BSS (the defualt stack is 4096 bytes)
	 * so we really are decompressing in place.
	 * 
	 * Hmm. I missed a trick.  In the very worst case (no compression)
	 * the encoded data is 9/8 the size as it started out so to be completely
	 * safe I need to be 1/8 of the uncompressed code size past the end.
	 * This will still fit compfortably into our bss in any conceivable scenario.
	 */
	_compressed_copy = _edata + _prefix_size  - _compressed_size +
		/* The amount to overflow _edata */
		MAX( ((_edata - _text + 7) / 8) , 2016 ) + 32; 
	_assert = ASSERT( ( _compressed_copy - _prefix_size ) < _ebss , "Cannot decompress in place" ) ; 

	decompress = DEFINED(decompress) ? decompress : 0;
	/DISCARD/ : {
		*(.comment)
		*(.note)
	}

	/* Symbols used by the prefixes whose addresses are inconvinient 
	 * to compute, at runtime in the code.
	 */
	image_basemem_size = DEFINED(image_basemem_size)? image_basemem_size : 65536;
	image_basemem      = DEFINED(image_basemem)?     image_basemem : 65536;
	_prefix_real_to_prot          = _real_to_prot         + _prefix_size ;
	_prefix_prot_to_real          = _prot_to_real         + _prefix_size ;
	_prefix_image_basemem_size    = image_basemem_size    + _prefix_size ;
	_prefix_image_basemem         = image_basemem         + _prefix_size ;
	_prefix_rm_in_call            = _rm_in_call           + _prefix_size ;
	_prefix_in_call               = _in_call              + _prefix_size ;
	_prefix_rom                   = rom                   + _prefix_size ;
	_prefix_rm_etherboot_location = rm_etherboot_location + _prefix_size ;
	_prefix_stack_end             = _stack_end            + _prefix_size ;
}
