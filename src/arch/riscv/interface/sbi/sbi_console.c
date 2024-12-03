/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

/** @file
 *
 * SBI debug console
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/sbi.h>
#include <ipxe/io.h>
#include <ipxe/console.h>
#include <config/console.h>

/* Set default console usage if applicable */
#if ! ( defined ( CONSOLE_SBI ) && CONSOLE_EXPLICIT ( CONSOLE_SBI ) )
#undef CONSOLE_SBI
#define CONSOLE_SBI ( CONSOLE_USAGE_ALL & ~CONSOLE_USAGE_LOG )
#endif

/** Buffered input character (if any) */
static unsigned char sbi_console_input;

/**
 * Print a character to SBI console
 *
 * @v character		Character to be printed
 */
static void sbi_putchar ( int character ) {

	/* Write byte to console */
	sbi_ecall_1 ( SBI_DBCN, SBI_DBCN_WRITE_BYTE, character );
}

/**
 * Get character from SBI console
 *
 * @ret character	Character read from console, if any
 */
static int sbi_getchar ( void ) {
	int character;

	/* Consume and return buffered character, if any */
	character = sbi_console_input;
	sbi_console_input = 0;
	return character;
}

/**
 * Check for character ready to read from SBI console
 *
 * @ret True		Character available to read
 * @ret False		No character available to read
 */
static int sbi_iskey ( void ) {
	struct sbi_return ret;

	/* Do nothing if we already have a buffered character */
	if ( sbi_console_input )
		return sbi_console_input;

	/* Read and buffer byte from console, if any */
	ret = sbi_ecall_3 ( SBI_DBCN, SBI_DBCN_READ,
			    sizeof ( sbi_console_input ),
			    virt_to_phys ( &sbi_console_input ), 0 );
	if ( ret.error )
		return 0;

	/* Return number of characters read and buffered */
	return ret.value;
}

/** SBI console */
struct console_driver sbi_console_driver __console_driver = {
	.putchar = sbi_putchar,
	.getchar = sbi_getchar,
	.iskey = sbi_iskey,
	.usage = CONSOLE_SBI,
};
