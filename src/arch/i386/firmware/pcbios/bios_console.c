/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <realmode.h>
#include <console.h>

/**
 * Print a character to BIOS console
 *
 * @v character		Character to be printed
 */
static void bios_putchar ( int character ) {

	__asm__ __volatile__ ( REAL_CODE ( "sti\n\t"
					   "int $0x10\n\t"
					   "cli\n\t" )
			       : : "a" ( character | 0x0e00 ), "b" ( 1 )
			       : "ebp" );
}

/**
 * Get character from BIOS console
 *
 * @ret character	Character read from console
 */
static int bios_getchar ( void ) {
	uint8_t character;
	
	__asm__ __volatile__ ( REAL_CODE ( "sti\n\t"
					   "int $0x16\n\t"
					   "cli\n\t" )
			       : "=a" ( character ) : "a" ( 0x0000 ) );

	return character;
}

/**
 * Check for character ready to read from BIOS console
 *
 * @ret True		Character available to read
 * @ret False		No character available to read
 */
static int bios_iskey ( void ) {
	unsigned int discard_a;
	unsigned int flags;
	
	__asm__ __volatile__ ( REAL_CODE ( "sti\n\t"
					   "int $0x16\n\t"
					   "pushfw\n\t"
					   "popw %w0\n\t"
					   "cli\n\t" )
			       : "=r" ( flags ), "=a" ( discard_a )
			       : "a" ( 0x0001 ) );
	
	return ( ! ( flags & ZF ) );
}

struct console_driver bios_console __console_driver = {
	.putchar = bios_putchar,
	.getchar = bios_getchar,
	.iskey = bios_iskey,
};
