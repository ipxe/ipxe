OUTPUT_FORMAT("elf64-ia64-little")

OUTPUT_ARCH(ia64)

ENTRY(_start)
SECTIONS {
	. = 0;
	__gp = . + 0x200000;
	_virt_start = .;
	_text = . ;
	.text : {
		/* Start address of etherboot in the virtual address space */
		*(.text)
		*(.text.*)
		_etext = . ;

		_rodata = . ;
		. = ALIGN(16);
		*(.rodata)
		*(.rodata.*)
		*(.srodata)
		. = ALIGN(16);
		pci_drivers = . ;
		*(.drivers.pci);
		pci_drivers_end = . ;
		. = ALIGN(16);
		isa_drivers = . ;
		*(.drivers.isa);
		isa_drivers_end = . ;
		. = ALIGN(16);

		. = ALIGN(16);
		_rela = . ;
		*(.rela.text)
		*(.rela.rodata)
		*(.rela.drivers.pci)
		*(.rela.drivers.isa)
		*(.rela.drivers.efi)
		*(.rela.data)
		*(.rela.sdata)
		*(.rela.got)
		_erela = . ;
		. = ALIGN(16);
		_erodata = . ;
	}
	_rela_size = _erela - _rela ; 
	.data : {
		_data = . ;
		*(.data)
		*(.got.plt)
		*(.got)
		*(.sdata)
		*(.sbss)
		*(.scommon)
		*(.data.*)
		*(.data1)
		. = ALIGN(16);
		_edata = . ;
	}
	_bss = . ;
	.bss : {
		*(.sbss)
		*(.scommon)
		*(.dynbss)
		*(.bss)
		*(COMMON)
	}
	_ebss = .;
	_end = .;
	/DISCARD/ : {
		*(.comment)
		*(.note)
		*(.hash)
		*(.dynstr)
		*(.dynsym)
		*(.IA_64.unwind)
		*(.IA_64.unwind_info)
		*(.IA64_unwind)
		*(.IA64_unwind_info)
		*(.dynamic)
	}
}
