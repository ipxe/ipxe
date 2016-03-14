/*
 * Copyright 2007 Red Hat, Inc.
 * Copyright 2016 Jonathan Dieter <jdieter@lesbg.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Author: Soren Sandmann <sandmann@redhat.com> */

FILE_LICENCE ( MIT );

#include <string.h>
#include <errno.h>
#include <edid.h>

/**
 * Get a single bit from an int
 *
 * @v   in		Int to extract bit from
 * @v   bit		Position of bit to extract
 * @ret value		Value of bit
 */
static int get_bit ( int in, int bit ) {
	return ( in & ( 1 << bit ) ) >> bit;
}

/**
 * Decode checksum
 *
 * @v   edid		String containing EDID
 * @ret rc		False if checksum is invalid
 */
static int decode_check_sum ( const unsigned char *edid ) {
	int i;
	unsigned char check = 0;

	for ( i = 0; i < 128; ++i )
		check += edid[i];

	/* Checksum should be 0 */
	if ( check ) {
		int rc = -EILSEQ;
		DBGC ( edid, "EDID checksum %d invalid - should be 0: %s\n",
		       check, strerror ( rc ) );
		return FALSE;
	} else
		return TRUE;
}

/**
 * Get preferred resolution from EDID
 *
 * @v   edid		EDID string
 * @v   x		X resolution of preferred resolution
 * @v   y		Y resolution of preferred resolution
 * @ret rc		Return status code
 */
int edid_get_preferred_resolution ( const unsigned char *edid,
				    unsigned int *x, unsigned int *y ) {
	if ( ! decode_check_sum ( edid ) )
		return FALSE;

	if ( get_bit ( edid[0x18], 1 ) ) {
		*x = edid[0x36 + 0x02] | ( ( edid[0x36 + 0x04] & 0xf0 ) << 4 );
		*y = edid[0x36 + 0x05] | ( ( edid[0x36 + 0x07] & 0xf0 ) << 4 );
		return TRUE;
	} else {
		return FALSE;
	}
}
