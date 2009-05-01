/*
 * Copyright (C) 2009 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <gpxe/base64.h>

/** @file
 *
 * Base64 encoding
 *
 */

static const char base64[64] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * Base64-encode a string
 *
 * @v raw		Raw string
 * @v encoded		Buffer for encoded string
 *
 * The buffer must be the correct length for the encoded string.  Use
 * something like
 *
 *     char buf[ base64_encoded_len ( strlen ( raw ) ) + 1 ];
 *
 * (the +1 is for the terminating NUL) to provide a buffer of the
 * correct size.
 */
void base64_encode ( const char *raw, char *encoded ) {
	const uint8_t *raw_bytes = ( ( const uint8_t * ) raw );
	uint8_t *encoded_bytes = ( ( uint8_t * ) encoded );
	size_t raw_bit_len = ( 8 * strlen ( raw ) );
	unsigned int bit;
	unsigned int tmp;

	for ( bit = 0 ; bit < raw_bit_len ; bit += 6 ) {
		tmp = ( ( raw_bytes[ bit / 8 ] << ( bit % 8 ) ) |
			( raw_bytes[ bit / 8 + 1 ] >> ( 8 - ( bit % 8 ) ) ) );
		tmp = ( ( tmp >> 2 ) & 0x3f );
		*(encoded_bytes++) = base64[tmp];
	}
	for ( ; ( bit % 8 ) != 0 ; bit += 6 )
		*(encoded_bytes++) = '=';
	*(encoded_bytes++) = '\0';

	DBG ( "Base64-encoded \"%s\" as \"%s\"\n", raw, encoded );
	assert ( strlen ( encoded ) == base64_encoded_len ( strlen ( raw ) ) );
}
