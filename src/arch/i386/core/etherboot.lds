OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_text)
SECTIONS {
	. = ALIGN(16);
	/* Start address of Etherboot in the virtual address space */
	_virt_start = 0;
	_text = . ;
	.text.nocompress : {
		*(.text*.nocompress)
		. = ALIGN(16);
	} = 0x9090

	.text16 : {
		_text16 = .;
		*(.text16)
		*(.text16.*)
		_etext16 = . ;
	}
	.text.compress : {
		*(.text)
		*(.text.*)
	} = 0x9090
	.rodata : {
		. = ALIGN(4);
		*(.rodata)
		*(.rodata.*)
	}
	. = ALIGN(4);
	.drivers.pci : {
		pci_drivers = . ;
		*(.drivers.pci);
		pci_drivers_end = . ;
	}
	. = ALIGN(4);
	.drivers.isa : {
		isa_drivers = . ;
		*(.drivers.isa);
		isa_drivers_end = .;
	}
	_etext = . ;
	_data = . ;
	.data : {
		*(.data)
		*(.data.*)
	}
	_edata = . ;
	_uncompressed_verbatim_end = . ;
	. = ALIGN(16);
	.bss.preserve : {
		*(.bss.preserve)
		*(.bss.preserve.*)
	}
	_bss = . ;
	.bss : {
		*(.bss)
		*(.bss.*)
	}
	. = ALIGN(16);
	_ebss = .;
	_stack = . ;
	.stack : {
		_stack_start = . ;
		*(.stack)
		*(.stack.*)
		_stack_end = . ;
	}
	_bss_size = _ebss - _bss;
	_stack_offset = _stack - _text ;
	_stack_offset_pgh = _stack_offset / 16 ;
	_stack_size = _stack_end - _stack_start ;
	. = ALIGN(16);
	_end = . ;

	/DISCARD/ : {
		*(.comment)
		*(.note)
	}

	/* PXE-specific symbol calculations.  The results of these are
	 * needed in romprefix.S, which is why they must be calculated
	 * here.
	 */
	_pxe_stack_size = _pxe_stack_t_size
		+ _pxe_callback_interface_size
		+ _rm_callback_interface_size
		+ _e820mangler_size + 15 ;

}
