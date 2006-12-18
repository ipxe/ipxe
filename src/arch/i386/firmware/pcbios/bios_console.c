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

#include <assert.h>
#include <realmode.h>
#include <console.h>
#include <gpxe/ansiesc.h>

#define ATTR_BOLD		0x08

#define ATTR_FCOL_MASK		0x07
#define ATTR_FCOL_BLACK		0x00
#define ATTR_FCOL_BLUE		0x01
#define ATTR_FCOL_GREEN		0x02
#define ATTR_FCOL_CYAN		0x03
#define ATTR_FCOL_RED		0x04
#define ATTR_FCOL_MAGENTA	0x05
#define ATTR_FCOL_YELLOW	0x06
#define ATTR_FCOL_WHITE		0x07

#define ATTR_BCOL_MASK		0x70
#define ATTR_BCOL_BLACK		0x00
#define ATTR_BCOL_BLUE		0x10
#define ATTR_BCOL_GREEN		0x20
#define ATTR_BCOL_CYAN		0x30
#define ATTR_BCOL_RED		0x40
#define ATTR_BCOL_MAGENTA	0x50
#define ATTR_BCOL_YELLOW	0x60
#define ATTR_BCOL_WHITE		0x70

#define ATTR_DEFAULT		ATTR_FCOL_WHITE

/** Current character attribute */
static unsigned int bios_attr = ATTR_DEFAULT;

/**
 * Handle ANSI CUP (cursor position)
 *
 * @v count		Parameter count
 * @v params[0]		Row (1 is top)
 * @v params[1]		Column (1 is left)
 */
static void bios_handle_cup ( unsigned int count __unused, int params[] ) {
	int cx = ( params[1] - 1 );
	int cy = ( params[0] - 1 );

	if ( cx < 0 )
		cx = 0;
	if ( cy < 0 )
		cy = 0;

	__asm__ __volatile__ ( REAL_CODE ( "sti\n\t"
					   "int $0x10\n\t"
					   "cli\n\t" )
			       : : "a" ( 0x0200 ), "b" ( 1 ),
			           "d" ( ( cy << 8 ) | cx ) );
}

/**
 * Handle ANSI ED (erase in page)
 *
 * @v count		Parameter count
 * @v params[0]		Region to erase
 */
static void bios_handle_ed ( unsigned int count __unused,
			     int params[] __unused ) {
	/* We assume that we always clear the whole screen */
	assert ( params[0] == ANSIESC_ED_ALL );

	__asm__ __volatile__ ( REAL_CODE ( "sti\n\t"
					   "int $0x10\n\t"
					   "cli\n\t" )
			       : : "a" ( 0x0600 ), "b" ( bios_attr << 8 ),
			           "c" ( 0 ), "d" ( 0xffff ) );
}

/**
 * Handle ANSI SGR (set graphics rendition)
 *
 * @v count		Parameter count
 * @v params		List of graphic rendition aspects
 */
static void bios_handle_sgr ( unsigned int count, int params[] ) {
	static const uint8_t bios_attr_fcols[10] = {
		ATTR_FCOL_BLACK, ATTR_FCOL_RED, ATTR_FCOL_GREEN,
		ATTR_FCOL_YELLOW, ATTR_FCOL_BLUE, ATTR_FCOL_MAGENTA,
		ATTR_FCOL_CYAN, ATTR_FCOL_WHITE,
		ATTR_FCOL_WHITE, ATTR_FCOL_WHITE /* defaults */
	};
	static const uint8_t bios_attr_bcols[10] = {
		ATTR_BCOL_BLACK, ATTR_BCOL_RED, ATTR_BCOL_GREEN,
		ATTR_BCOL_YELLOW, ATTR_BCOL_BLUE, ATTR_BCOL_MAGENTA,
		ATTR_BCOL_CYAN, ATTR_BCOL_WHITE,
		ATTR_BCOL_BLACK, ATTR_BCOL_BLACK /* defaults */
	};
	unsigned int i;
	int aspect;

	for ( i = 0 ; i < count ; i++ ) {
		aspect = params[i];
		if ( aspect == 0 ) {
			bios_attr = ATTR_DEFAULT;
		} else if ( aspect == 1 ) {
			bios_attr |= ATTR_BOLD;
		} else if ( aspect == 22 ) {
			bios_attr &= ~ATTR_BOLD;
		} else if ( ( aspect >= 30 ) && ( aspect <= 39 ) ) {
			bios_attr &= ~ATTR_FCOL_MASK;
			bios_attr |= bios_attr_fcols[ aspect - 30 ];
		} else if ( ( aspect >= 40 ) && ( aspect <= 49 ) ) {
			bios_attr &= ~ATTR_BCOL_MASK;
			bios_attr |= bios_attr_bcols[ aspect - 40 ];
		}
	}
}

/** BIOS console ANSI escape sequence handlers */
static struct ansiesc_handler bios_ansiesc_handlers[] = {
	{ ANSIESC_CUP, bios_handle_cup },
	{ ANSIESC_ED, bios_handle_ed },
	{ ANSIESC_SGR, bios_handle_sgr },
	{ 0, NULL }
};

/** BIOS console ANSI escape sequence context */
static struct ansiesc_context bios_ansiesc_ctx = {
	.handlers = bios_ansiesc_handlers,
};

/**
 * Print a character to BIOS console
 *
 * @v character		Character to be printed
 */
static void bios_putchar ( int character ) {

	/* Intercept ANSI escape sequences */
	character = ansiesc_process ( &bios_ansiesc_ctx, character );
	if ( character < 0 )
		return;

	/* Set attribute for printable characters */
	if ( character >= 0x20 ) {
		__asm__ __volatile__ ( REAL_CODE ( "sti\n\t"
						   "int $0x10\n\t"
						   "cli\n\t" )
				       : : "a" ( 0x0920 ), "b" ( bios_attr ),
				           "c" ( 1 ) );
	}

	/* Print the character */
	__asm__ __volatile__ ( REAL_CODE ( "sti\n\t"
					   "int $0x10\n\t"
					   "cli\n\t" )
			       : : "a" ( character | 0x0e00 ), "b" ( 0 )
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
			       : "a" ( 0x0100 ) );
	
	return ( ! ( flags & ZF ) );
}

struct console_driver bios_console __console_driver = {
	.putchar = bios_putchar,
	.getchar = bios_getchar,
	.iskey = bios_iskey,
};
