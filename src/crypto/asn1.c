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

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <ipxe/tables.h>
#include <ipxe/asn1.h>

/** @file
 *
 * ASN.1 encoding
 *
 */

/* Disambiguate the various error causes */
#define EINVAL_ASN1_EMPTY \
	__einfo_error ( EINFO_EINVAL_ASN1_EMPTY )
#define EINFO_EINVAL_ASN1_EMPTY \
	__einfo_uniqify ( EINFO_EINVAL, 0x01, "Empty or underlength cursor" )
#define EINVAL_ASN1_LEN_LEN \
	__einfo_error ( EINFO_EINVAL_ASN1_LEN_LEN )
#define EINFO_EINVAL_ASN1_LEN_LEN \
	__einfo_uniqify ( EINFO_EINVAL, 0x02, "Length field overruns cursor" )
#define EINVAL_ASN1_LEN \
	__einfo_error ( EINFO_EINVAL_ASN1_LEN )
#define EINFO_EINVAL_ASN1_LEN \
	__einfo_uniqify ( EINFO_EINVAL, 0x03, "Field overruns cursor" )
#define EINVAL_ASN1_BOOLEAN \
	__einfo_error ( EINFO_EINVAL_ASN1_BOOLEAN )
#define EINFO_EINVAL_ASN1_BOOLEAN \
	__einfo_uniqify ( EINFO_EINVAL, 0x04, "Invalid boolean" )
#define EINVAL_ASN1_INTEGER \
	__einfo_error ( EINFO_EINVAL_ASN1_INTEGER )
#define EINFO_EINVAL_ASN1_INTEGER \
	__einfo_uniqify ( EINFO_EINVAL, 0x04, "Invalid integer" )

/**
 * Invalidate ASN.1 object cursor
 *
 * @v cursor		ASN.1 object cursor
 */
void asn1_invalidate_cursor ( struct asn1_cursor *cursor ) {
	static uint8_t asn1_invalid_object[] = { ASN1_END, 0 };

	cursor->data = asn1_invalid_object;
	cursor->len = 0;
}

/**
 * Start parsing ASN.1 object
 *
 * @v cursor		ASN.1 object cursor
 * @v type		Expected type, or ASN1_ANY
 * @ret len		Length of object body, or negative error
 *
 * The object cursor will be updated to point to the start of the
 * object body (i.e. the first byte following the length byte(s)), and
 * the length of the object body (i.e. the number of bytes until the
 * following object tag, if any) is returned.
 */
static int asn1_start ( struct asn1_cursor *cursor, unsigned int type ) {
	unsigned int len_len;
	unsigned int len;

	/* Sanity check */
	if ( cursor->len < 2 /* Tag byte and first length byte */ ) {
		if ( cursor->len )
			DBGC ( cursor, "ASN1 %p too short\n", cursor );
		return -EINVAL_ASN1_EMPTY;
	}

	/* Check the tag byte */
	if ( ( type != ASN1_ANY ) && ( type != asn1_type ( cursor ) ) ) {
		DBGC ( cursor, "ASN1 %p type mismatch (expected %d, got %d)\n",
		       cursor, type, *( ( uint8_t * ) cursor->data ) );
		return -ENXIO;
	}
	cursor->data++;
	cursor->len--;

	/* Extract length of the length field and sanity check */
	len_len = *( ( uint8_t * ) cursor->data );
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
		return -EINVAL_ASN1_LEN_LEN;
	}

	/* Extract the length and sanity check */
	for ( len = 0 ; len_len ; len_len-- ) {
		len <<= 8;
		len |= *( ( uint8_t * ) cursor->data );
		cursor->data++;
		cursor->len--;
	}
	if ( cursor->len < len ) {
		DBGC ( cursor, "ASN1 %p bad length %d (max %zd)\n",
		       cursor, len, cursor->len );
		return -EINVAL_ASN1_LEN;
	}

	return len;
}

/**
 * Enter ASN.1 object
 *
 * @v cursor		ASN.1 object cursor
 * @v type		Expected type, or ASN1_ANY
 * @ret rc		Return status code
 *
 * The object cursor will be updated to point to the body of the
 * current ASN.1 object.  If any error occurs, the object cursor will
 * be invalidated.
 */
int asn1_enter ( struct asn1_cursor *cursor, unsigned int type ) {
	int len;

	len = asn1_start ( cursor, type );
	if ( len < 0 ) {
		asn1_invalidate_cursor ( cursor );
		return len;
	}

	cursor->len = len;
	DBGC ( cursor, "ASN1 %p entered object type %02x (len %x)\n",
	       cursor, type, len );

	return 0;
}

/**
 * Skip ASN.1 object if present
 *
 * @v cursor		ASN.1 object cursor
 * @v type		Expected type, or ASN1_ANY
 * @ret rc		Return status code
 *
 * The object cursor will be updated to point to the next ASN.1
 * object.  If any error occurs, the object cursor will not be
 * modified.
 */
int asn1_skip_if_exists ( struct asn1_cursor *cursor, unsigned int type ) {
	int len;

	len = asn1_start ( cursor, type );
	if ( len < 0 )
		return len;

	cursor->data += len;
	cursor->len -= len;
	DBGC ( cursor, "ASN1 %p skipped object type %02x (len %x)\n",
	       cursor, type, len );

	if ( ! cursor->len ) {
		DBGC ( cursor, "ASN1 %p reached end of object\n", cursor );
		return -ENOENT;
	}

	return 0;
}

/**
 * Skip ASN.1 object
 *
 * @v cursor		ASN.1 object cursor
 * @v type		Expected type, or ASN1_ANY
 * @ret rc		Return status code
 *
 * The object cursor will be updated to point to the next ASN.1
 * object.  If any error occurs, the object cursor will be
 * invalidated.
 */
int asn1_skip ( struct asn1_cursor *cursor, unsigned int type ) {
	int rc;

	if ( ( rc = asn1_skip_if_exists ( cursor, type ) ) != 0 ) {
		asn1_invalidate_cursor ( cursor );
		return rc;
	}

	return 0;
}

/**
 * Shrink ASN.1 cursor to fit object
 *
 * @v cursor		ASN.1 object cursor
 * @v type		Expected type, or ASN1_ANY
 * @ret rc		Return status code
 *
 * The object cursor will be shrunk to contain only the current ASN.1
 * object.  If any error occurs, the object cursor will be
 * invalidated.
 */
int asn1_shrink ( struct asn1_cursor *cursor, unsigned int type ) {
	struct asn1_cursor temp;
	const void *end;
	int len;

	/* Find end of object */
	memcpy ( &temp, cursor, sizeof ( temp ) );
	len = asn1_start ( &temp, type );
	if ( len < 0 ) {
		asn1_invalidate_cursor ( cursor );
		return len;
	}
	end = ( temp.data + len );

	/* Shrink original cursor to contain only its first object */
	cursor->len = ( end - cursor->data );

	return 0;
}

/**
 * Enter ASN.1 object of any type
 *
 * @v cursor		ASN.1 object cursor
 * @ret rc		Return status code
 */
int asn1_enter_any ( struct asn1_cursor *cursor ) {
	return asn1_enter ( cursor, ASN1_ANY );
}

/**
 * Skip ASN.1 object of any type
 *
 * @v cursor		ASN.1 object cursor
 * @ret rc		Return status code
 */
int asn1_skip_any ( struct asn1_cursor *cursor ) {
	return asn1_skip ( cursor, ASN1_ANY );
}

/**
 * Shrink ASN.1 object of any type
 *
 * @v cursor		ASN.1 object cursor
 * @ret rc		Return status code
 */
int asn1_shrink_any ( struct asn1_cursor *cursor ) {
	return asn1_shrink ( cursor, ASN1_ANY );
}

/**
 * Parse value of ASN.1 boolean
 *
 * @v cursor		ASN.1 object cursor
 * @ret value		Value, or negative error
 */
int asn1_boolean ( const struct asn1_cursor *cursor ) {
	struct asn1_cursor contents;
	const struct asn1_boolean *boolean;

	/* Enter boolean */
	memcpy ( &contents, cursor, sizeof ( contents ) );
	asn1_enter ( &contents, ASN1_BOOLEAN );
	if ( contents.len != sizeof ( *boolean ) )
		return -EINVAL_ASN1_BOOLEAN;

	/* Extract value */
	boolean = contents.data;
	return boolean->value;
}

/**
 * Parse value of ASN.1 integer
 *
 * @v cursor		ASN.1 object cursor
 * @v value		Value to fill in
 * @ret rc		Return status code
 */
int asn1_integer ( const struct asn1_cursor *cursor, int *value ) {
	struct asn1_cursor contents;
	uint8_t high_byte;
	int rc;

	/* Enter integer */
	memcpy ( &contents, cursor, sizeof ( contents ) );
	if ( ( rc = asn1_enter ( &contents, ASN1_INTEGER ) ) != 0 )
		return rc;
	if ( contents.len < 1 )
		return -EINVAL_ASN1_INTEGER;

	/* Initialise value according to sign byte */
	*value = *( ( int8_t * ) contents.data );
	contents.data++;
	contents.len--;

	/* Process value */
	while ( contents.len ) {
		high_byte = ( (*value) >> ( 8 * ( sizeof ( *value ) - 1 ) ) );
		if ( ( high_byte != 0x00 ) && ( high_byte != 0xff ) ) {
			DBGC ( cursor, "ASN1 %p integer overflow\n", cursor );
			return -EINVAL_ASN1_INTEGER;
		}
		*value = ( ( *value << 8 ) | *( ( uint8_t * ) contents.data ) );
		contents.data++;
		contents.len--;
	}

	return 0;
}

/**
 * Compare two ASN.1 objects
 *
 * @v cursor1		ASN.1 object cursor
 * @v cursor2		ASN.1 object cursor
 * @ret difference	Difference as returned by memcmp()
 *
 * Note that invalid and empty cursors will compare as equal with each
 * other.
 */
int asn1_compare ( const struct asn1_cursor *cursor1,
		   const struct asn1_cursor *cursor2 ) {
	int difference;

	difference = ( cursor2->len - cursor1->len );
	return ( difference ? difference :
		 memcmp ( cursor1->data, cursor2->data, cursor1->len ) );
}

/**
 * Identify ASN.1 algorithm by OID
 *
 * @v cursor		ASN.1 object cursor

 * @ret algorithm	Algorithm, or NULL
 */
static struct asn1_algorithm *
asn1_find_algorithm ( const struct asn1_cursor *cursor ) {
	struct asn1_algorithm *algorithm;

	for_each_table_entry ( algorithm, ASN1_ALGORITHMS ) {
		if ( asn1_compare ( &algorithm->oid, cursor ) == 0 )
			return algorithm;
	}

	return NULL;
}

/**
 * Parse ASN.1 OID-identified algorithm
 *
 * @v cursor		ASN.1 object cursor
 * @ret algorithm	Algorithm, or NULL
 */
struct asn1_algorithm * asn1_algorithm ( const struct asn1_cursor *cursor ) {
	struct asn1_cursor contents;
	struct asn1_algorithm *algorithm;
	int rc;

	/* Enter signatureAlgorithm */
	memcpy ( &contents, cursor, sizeof ( contents ) );
	asn1_enter ( &contents, ASN1_SEQUENCE );

	/* Enter algorithm */
	if ( ( rc = asn1_enter ( &contents, ASN1_OID ) ) != 0 ) {
		DBGC ( cursor, "ASN1 %p cannot locate algorithm OID:\n",
		       cursor );
		DBGC_HDA ( cursor, 0, cursor->data, cursor->len );
		return NULL;
	}

	/* Identify algorithm */
	algorithm = asn1_find_algorithm ( &contents );
	if ( ! algorithm ) {
		DBGC ( cursor, "ASN1 %p unrecognised algorithm:\n", cursor );
		DBGC_HDA ( cursor, 0, cursor->data, cursor->len );
		return NULL;
	}

	return algorithm;
}
