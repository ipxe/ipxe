/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
SECTIONS
{
	/*. = 0x00000000;*/	/* PIC */
	/*. = 0x00000400;*/	/* ROM Bootloader */
	. = 0x40000000;		/* SDRAM */

	. = ALIGN(4);
	_text = . ;
	.text      :
	{
	  _start = .;
	  _virt_start = .;
	  bin/start.o	(.text)
	  *(.text)

	  . = ALIGN(16);
	  isa_drivers = . ;
	  *(.drivers.isa);
	  isa_drivers_end = . ;	  
	}

	. = ALIGN(4);
	.rodata : { *(.rodata) }

	. = ALIGN(4);
	.data : { *(.data) }

	. = ALIGN(4);
	.got : { *(.got) }

	. = ALIGN(4);
	_bss = . ;
	.bss : { *(.bss) }

	. = ALIGN(4);
	_ebss = .;
	_end = .;

	. = ALIGN(16);
	.text :
	{
		*(.dma.desc);
		*(.dma.buffer);
	}
}
