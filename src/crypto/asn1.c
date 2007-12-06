/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <gpxe/asn1.h>

/** @file
 *
 * ASN.1 encoding
 *
 */

/**
 * Start parsing ASN.1 object
 *
 * @v cursor		ASN.1 object cursor
 * @v type		Expected type
 * @ret len		Length of object body, or -1 on error
 *
 * The object cursor will be updated to point to the start of the
 * object body (i.e. the first byte following the length byte(s)), and
 * the length of the object body (i.e. the number of bytes until the
 * following object tag, if any) is returned.
 *
 * If any error occurs (i.e. if the object is not of the expected
 * type, or if we overflow beyond the end of the ASN.1 object), then
 * the cursor will be invalidated and a negative value will be
 * returned.
 */
static int asn1_start_object ( struct asn1_cursor *cursor,
			       unsigned int type ) {
	unsigned int len_len;
	unsigned int len;

	/* Sanity check */
	if ( cursor->len < 2 /* Tag byte and first length byte */ ) {
		if ( cursor->len )
			DBGC ( cursor, "ASN1 %p too short\n", cursor );
		goto notfound;
	}

	/* Check the tag byte */
	if ( cursor->data[0] != type ) {
		DBGC ( cursor, "ASN1 %p type mismatch (expected %d, got %d)\n",
		       cursor, type, cursor->data[0] );
		goto notfound;
	}
	cursor->data++;
	cursor->len--;

	/* Extract length of the length field and sanity check */
	len_len = cursor->data[0];
	if ( len_len & 0x80 ) {
		len_len = ( len_len & 0x7f );
		cursor->data++;
		cursor->len--;
	} else {
		len_len = 1;
	}
	if ( cursor->len < len_len ) {
		DBGC ( cursor, "ASN1 %p bad length field length %d (max "
		       "%zd)\n", cursor, len_len, cursor->len );
		goto notfound;
	}

	/* Extract the length and sanity check */
	for ( len = 0 ; len_len ; len_len-- ) {
		len <<= 8;
		len |= cursor->data[0];
		cursor->data++;
		cursor->len--;
	}
	if ( cursor->len < len ) {
		DBGC ( cursor, "ASN1 %p bad length %d (max %zd)\n",
		       cursor, len, cursor->len );
		goto notfound;
	}

	return len;

 notfound:
	cursor->data = NULL;
	cursor->len = 0;
	return -1;
}

/**
 * Enter ASN.1 object
 *
 * @v cursor		ASN.1 object cursor
 * @v type		Expected type
 * @ret rc		Return status code
 *
 * The object cursor will be updated to point to the body of the
 * current ASN.1 object.  If any error occurs, the object cursor will
 * be invalidated.
 */
int asn1_enter_object ( struct asn1_cursor *cursor, unsigned int type ) {
	int len;

	len = asn1_start_object ( cursor, type );
	if ( len < 0 )
		return -ENOENT;

	cursor->len = len;
	DBGC ( cursor, "ASN1 %p entered object type %02x (len %x)\n",
	       cursor, type, len );

	return 0;
}

/**
 * Skip ASN.1 object
 *
 * @v cursor		ASN.1 object cursor
 * @v type		Expected type
 * @ret rc		Return status code
 *
 * The object cursor will be updated to point to the next ASN.1
 * object.  If any error occurs, the object cursor will be
 * invalidated.
 */
int asn1_skip_object ( struct asn1_cursor *cursor, unsigned int type ) {
	int len;

	len = asn1_start_object ( cursor, type );
	if ( len < 0 )
		return -ENOENT;

	cursor->data += len;
	cursor->len -= len;
	DBGC ( cursor, "ASN1 %p skipped object type %02x (len %x)\n",
	       cursor, type, len );

	if ( ! cursor->len ) {
		DBGC ( cursor, "ASN1 %p reached end of object\n", cursor );
		cursor->data = NULL;
		return -ENOENT;
	}

	return 0;
}
