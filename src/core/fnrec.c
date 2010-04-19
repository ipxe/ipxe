/*
 * Copyright (C) 2010 Stefan Hajnoczi <stefanha@gmail.com>.
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

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ipxe/init.h>
#include <ipxe/uaccess.h>

/** @file
 *
 * Function trace recorder for crash and hang debugging
 *
 */

enum {
	/** Constant for identifying valid trace buffers */
	fnrec_magic = 'f' << 24 | 'n' << 16 | 'r' << 8 | 'e',

	/** Trace buffer length */
	fnrec_buffer_length = 4096 / sizeof ( unsigned long ),
};

/** A trace buffer */
struct fnrec_buffer {
	/** Constant for identifying valid trace buffers */
	uint32_t magic;

	/** Next trace buffer entry to fill */
	uint32_t idx;

	/** Function address trace buffer */
	unsigned long data[fnrec_buffer_length];
};

/** The trace buffer */
static struct fnrec_buffer *fnrec_buffer;

/**
 * Test whether the trace buffer is valid
 *
 * @ret is_valid	Buffer is valid
 */
static int fnrec_is_valid ( void ) {
	return fnrec_buffer && fnrec_buffer->magic == fnrec_magic;
}

/**
 * Reset the trace buffer and clear entries
 */
static void fnrec_reset ( void ) {
	memset ( fnrec_buffer, 0, sizeof ( *fnrec_buffer ) );
	fnrec_buffer->magic = fnrec_magic;
}

/**
 * Write a value to the end of the buffer if it is not a repetition
 *
 * @v l			Value to append
 */
static void fnrec_append_unique ( unsigned long l ) {
	static unsigned long lastval;
	uint32_t idx = fnrec_buffer->idx;

	/* Avoid recording the same value repeatedly */
	if ( l == lastval )
		return;

	fnrec_buffer->data[idx] = l;
	fnrec_buffer->idx = ( idx + 1 ) % fnrec_buffer_length;
	lastval = l;
}

/**
 * Print the contents of the trace buffer in chronological order
 */
static void fnrec_dump ( void ) {
	size_t i;

	if ( !fnrec_is_valid() ) {
		printf ( "fnrec buffer not found\n" );
		return;
	}

	printf ( "fnrec buffer dump:\n" );
	for ( i = 0; i < fnrec_buffer_length; i++ ) {
		unsigned long l = fnrec_buffer->data[
			( fnrec_buffer->idx + i ) % fnrec_buffer_length];
		printf ( "%08lx%c", l, i % 8 == 7 ? '\n' : ' ' );
	}
}

/**
 * Function tracer initialisation function
 */
static void fnrec_init ( void ) {
	/* Hardcoded to 17 MB */
	fnrec_buffer = phys_to_virt ( 17 * 1024 * 1024 );
	fnrec_dump();
	fnrec_reset();
}

struct init_fn fnrec_init_fn __init_fn ( INIT_NORMAL ) = {
	.initialise = fnrec_init,
};

/*
 * These functions are called from every C function.  The compiler inserts
 * these calls when -finstrument-functions is used.
 */
void __cyg_profile_func_enter ( void *called_fn, void *call_site __unused ) {
	if ( fnrec_is_valid() )
		fnrec_append_unique ( ( unsigned long ) called_fn );
}

void __cyg_profile_func_exit ( void *called_fn __unused, void *call_site __unused ) {
}
