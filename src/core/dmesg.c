/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
FILE_SECBOOT ( PERMITTED );

#include <errno.h>
#include <ipxe/ansiesc.h>
#include <ipxe/settings.h>
#include <ipxe/console.h>

/** @file
 *
 * In-memory ring buffer console
 *
 */

/* Set default console usage if applicable */
#if ! ( defined ( CONSOLE_DMESG ) && CONSOLE_EXPLICIT ( CONSOLE_DMESG ) )
#undef CONSOLE_DMESG
#define CONSOLE_DMESG ( CONSOLE_USAGE_ALL & ~CONSOLE_USAGE_TUI )
#endif

/** Size of ring buffer */
#define DMESG_LEN 8192

/** Ring buffer */
static char dmesg[DMESG_LEN];

/** Offset within ring buffer */
static unsigned int dmesg_offset;

/** Ring buffer ANSI escape sequence handlers */
static struct ansiesc_handler dmesg_ansiesc_handlers[] = {
	{ 0, NULL }
};

/** Ring buffer ANSI escape sequence context */
static struct ansiesc_context dmesg_ansiesc_ctx = {
	.handlers = dmesg_ansiesc_handlers,
};

/**
 * Print a character to the ring buffer
 *
 * @v character		Character to be printed
 */
static void dmesg_putchar ( int character ) {

	/* Strip ANSI escape sequences */
	character = ansiesc_process ( &dmesg_ansiesc_ctx, character );
	if ( character < 0 )
		return;

	/* Handle backspace characters */
	if ( character == '\b' ) {
		if ( dmesg_offset )
			dmesg_offset--;
		return;
	}

	/* Record character */
	dmesg[ dmesg_offset++ % DMESG_LEN ] = character;
}

/** Ring buffer console driver */
struct console_driver dmesg_console __console_driver = {
	.putchar = dmesg_putchar,
	.usage = CONSOLE_DMESG,
};

/**
 * Fetch ring buffer setting
 *
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int dmesg_fetch ( void *data, size_t len ) {
	uint8_t *bytes = data;
	size_t max;
	unsigned int offset;

	/* Calculate length */
	max = dmesg_offset;
	if ( max > DMESG_LEN )
		max = DMESG_LEN;
	if ( len > max )
		len = max;

	/* Copy data */
	offset = ( dmesg_offset - len );
	while ( len-- )
		*(bytes++) = dmesg[ offset++ % DMESG_LEN ];

	return max;
}

/**
 * Store ring buffer setting
 *
 * @v data		Setting data, or NULL to clear setting
 * @v len		Length of setting data
 * @ret rc		Return status code
 */
static int dmesg_store ( const void *data __unused, size_t len ) {

	/* Only clearing the log is supported */
	if ( len )
		return -ENOTSUP;

	/* Clear log */
	dmesg_offset = 0;

	return 0;
}

/** Ring buffer setting */
const struct setting dmesg_setting __setting ( SETTING_MISC, dmesg ) = {
	.name = "dmesg",
	.description = "Ring buffer",
	.type = &setting_type_string,
	.scope = &builtin_scope,
};

/** Ring buffer built-in setting */
struct builtin_setting dmesg_builtin_setting __builtin_setting = {
	.setting = &dmesg_setting,
	.fetch = dmesg_fetch,
	.store = dmesg_store,
};
