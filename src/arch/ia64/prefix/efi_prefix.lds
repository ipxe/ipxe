/* OUTPUT_FORMAT("binary") */
OUTPUT_FORMAT("elf64-ia64-little")

OUTPUT_ARCH(ia64)

ENTRY(_start_plabel)
_sect_align = 16; /* normally 512 */
_file_align = 16; /* normally 4096 */
/* Symbols for hardcoding the payload and _bss size, with apply_efi_prefix
 * there is no need to set these, and in will get confused if these are not 0.
 */
_payload_size = 0;
_payload_bss = 0;
SECTIONS {
	/* We can arbitrarily set image base to anything we want,
	 * but efi does not honor it, so it is a pointless exercise.
	 * So we just set the start address to 0.
	 */
	. = 0;
	_link_base = . ;
	_image_base = . ;
	.hdrs : {
		_hdrs = . ;
		*(.hdrs)
		. = ALIGN(_file_align) ;
		_ehdrs = . ;
	}
	. = ALIGN(_sect_align);
	.img : {
		_img = . ;
		_text = . ;
		__gp = . ;
		*(.text)
		_etext = .;
		. = ALIGN(16);
		_data = . ;
		*(.data)
		_edata = .;
		. = ALIGN(16);
		_reloc = . ;
		*(.reloc)
		__ereloc = . ;
		_ereloc = . ;
		. = ALIGN(16);
		/* . = ALIGN(_file_align) ; */
	}
	_payload_start = . ;
	. = . + _payload_size ;
	_payload_end = . ;
	_eimg = . ;
	. = ALIGN(_sect_align) ; 
	_bss = . ;
	.bss : {
		*(.bss)
		. = . + _payload_bss;
	}
	_ebss = . ;
	_end = . ;
	/DISCARD/ : {
		*(*)
	}

	_hdrs_size = _ehdrs - _hdrs;
	_hdrs_off = 0;

	_text_size = _etext - _text ;
	_text_rva = _text - _image_base ;
	_text_off = _text - _link_base;
	
	_data_size = _edata - _data ;
	_data_rva = _data - _image_base;
	_data_off = _data - _link_base;

	__reloc_size = __ereloc - _reloc ;
	_reloc_size = _ereloc - _reloc ;
	_reloc_rva = _reloc - _image_base;
	_reloc_off = _reloc - _link_base;

	_bss_size = _ebss - _bss;
	_bss_rva = _bss - _image_base;

	_img_size = _eimg - _img ;
	_img_rva = _img - _image_base;
	_img_off = _img - _link_base;

	_img_mem_size = _ebss - _img;
	
	_image_size = _ebss - _link_base ;

	_start_plabel_rva = _start_plabel - _image_base;
}
