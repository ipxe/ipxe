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

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Message printing
 *
 */

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <ipxe/ansicol.h>
#include <ipxe/message.h>

/**
 * Print message centred on specified row
 *
 * @v row		Row
 * @v fmt		printf() format string
 * @v args		printf() argument list
 */
static void vmsg ( unsigned int row, const char *fmt, va_list args ) {
	char buf[COLS];
	size_t len;

	len = vsnprintf ( buf, sizeof ( buf ), fmt, args );
	mvprintw ( row, ( ( COLS - len ) / 2 ), "%s", buf );
}

/**
 * Print message centred on specified row
 *
 * @v row		Row
 * @v fmt		printf() format string
 * @v ..		printf() arguments
 */
void msg ( unsigned int row, const char *fmt, ... ) {
	va_list args;

	va_start ( args, fmt );
	vmsg ( row, fmt, args );
	va_end ( args );
}

/**
 * Clear message on specified row
 *
 * @v row		Row
 */
void clearmsg ( unsigned int row ) {
	move ( row, 0 );
	clrtoeol();
}

/**
 * Show alert message
 *
 * @v row		Row
 * @v fmt		printf() format string
 * @v args		printf() argument list
 */
static void valert ( unsigned int row, const char *fmt, va_list args ) {

	clearmsg ( row );
	color_set ( CPAIR_ALERT, NULL );
	vmsg ( row, fmt, args );
	sleep ( 2 );
	color_set ( CPAIR_NORMAL, NULL );
	clearmsg ( row );
}

/**
 * Show alert message
 *
 * @v row		Row
 * @v fmt		printf() format string
 * @v ...		printf() arguments
 */
void alert ( unsigned int row, const char *fmt, ... ) {
	va_list args;

	va_start ( args, fmt );
	valert ( row, fmt, args );
	va_end ( args );
}
