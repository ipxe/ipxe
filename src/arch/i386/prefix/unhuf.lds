OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

SECTIONS
{
	. = 0;
	.text : { 
		_text = .;
		*(.head)
		*(.text) 
	} = 0x9090
	.rodata : {
		*(.rodata)
	}
	_etext = . ;
	.data : { 
		*(.data) 
		/* Force 4 byte alignment */
		. = ALIGN(4);
		_payload = . ;
		*(.huf)
		_epayload = . ;
	}
	_edata = . ;
	_data_size = _edata - _start;
	/* Etherboot needs to be 16 byte aligned */
	. = ALIGN(16);
	.bss : { 
		*(.bss) 
	}
	_end = . ;
	_image_size = _end - _start;
}
