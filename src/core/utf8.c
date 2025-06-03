/*
 * Copyright (C) 2022 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <assert.h>
#include <ipxe/utf8.h>

/** @file
 *
 * UTF-8 Unicode encoding
 *
 */

/**
 * Accumulate Unicode character from UTF-8 byte sequence
 *
 * @v utf8		UTF-8 accumulator
 * @v byte		UTF-8 byte
 * @ret character	Unicode character, or 0 if incomplete
 */
unsigned int utf8_accumulate ( struct utf8_accumulator *utf8, uint8_t byte ) {
	static unsigned int min[] = {
		UTF8_MIN_TWO,
		UTF8_MIN_THREE,
		UTF8_MIN_FOUR,
	};
	unsigned int shift;
	unsigned int len;
	uint8_t tmp;

	/* Handle continuation bytes */
	if ( UTF8_IS_CONTINUATION ( byte ) ) {

		/* Fail if this is an unexpected continuation byte */
		if ( utf8->remaining == 0 ) {
			DBGC ( utf8, "UTF8 %p unexpected %02x\n", utf8, byte );
			return UTF8_INVALID;
		}

		/* Apply continuation byte */
		utf8->character <<= UTF8_CONTINUATION_BITS;
		utf8->character |= ( byte & UTF8_CONTINUATION_MASK );

		/* Return 0 if more continuation bytes are expected */
		if ( --utf8->remaining != 0 )
			return 0;

		/* Fail if sequence is illegal */
		if ( utf8->character < utf8->min ) {
			DBGC ( utf8, "UTF8 %p illegal %02x\n", utf8,
			       utf8->character );
			return UTF8_INVALID;
		}

		/* Sanity check */
		assert ( utf8->character != 0 );

		/* Return completed character */
		DBGC2 ( utf8, "UTF8 %p accumulated %02x\n",
			utf8, utf8->character );
		return utf8->character;
	}

	/* Reset state and report failure if this is an unexpected
	 * non-continuation byte.  Do not return UTF8_INVALID since
	 * doing so could cause us to drop a valid ASCII character.
	 */
	if ( utf8->remaining != 0 ) {
		shift = ( utf8->remaining * UTF8_CONTINUATION_BITS );
		DBGC ( utf8, "UTF8 %p unexpected %02x (partial %02x/%02x)\n",
		       utf8, byte, ( utf8->character << shift ),
		       ( ( 1 << shift ) - 1 ) );
		utf8->remaining = 0;
	}

	/* Handle initial bytes */
	if ( ! UTF8_IS_ASCII ( byte ) ) {

		/* Sanity check */
		assert ( utf8->remaining == 0 );

		/* Count total number of bytes in sequence */
		tmp = byte;
		len = 0;
		while ( tmp & UTF8_HIGH_BIT ) {
			tmp <<= 1;
			len++;
		}

		/* Check for illegal length */
		if ( len > UTF8_MAX_LEN ) {
			DBGC ( utf8, "UTF8 %p illegal %02x length %d\n",
			       utf8, byte, len );
			return UTF8_INVALID;
		}

		/* Store initial bits of character */
		utf8->character = ( tmp >> len );

		/* Store number of bytes remaining */
		len--;
		utf8->remaining = len;
		assert ( utf8->remaining > 0 );

		/* Store minimum legal value */
		utf8->min = min[ len - 1 ];
		assert ( utf8->min > 0 );

		/* Await continuation bytes */
		return 0;
	}

	/* Handle ASCII bytes */
	return byte;
}
