#ifndef _IPXE_ASN1_H
#define _IPXE_ASN1_H

/** @file
 *
 * ASN.1 encoding
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <ipxe/tables.h>

/** An ASN.1 object cursor */
struct asn1_cursor {
	/** Start of data */
	const void *data;
	/** Length of data */
	size_t len;
};

/** ASN.1 end */
#define ASN1_END 0x00

/** ASN.1 boolean */
#define ASN1_BOOLEAN 0x01

/** ASN.1 integer */
#define ASN1_INTEGER 0x02

/** ASN.1 bit string */
#define ASN1_BIT_STRING 0x03

/** ASN.1 octet string */
#define ASN1_OCTET_STRING 0x04

/** ASN.1 null */
#define ASN1_NULL 0x05

/** ASN.1 object identifier */
#define ASN1_OID 0x06

/** ASN.1 UTC time */
#define ASN1_UTC_TIME 0x17

/** ASN.1 generalized time */
#define ASN1_GENERALIZED_TIME 0x18

/** ASN.1 sequence */
#define ASN1_SEQUENCE 0x30

/** ASN.1 set */
#define ASN1_SET 0x31

/** ASN.1 explicit tag */
#define ASN1_EXPLICIT_TAG( number) ( 0xa0 | (number) )

/** ASN.1 "any tag" magic value */
#define ASN1_ANY -1U

/** Initial OID byte */
#define ASN1_OID_INITIAL( first, second ) ( ( (first) * 40 ) + (second) )

/** Single-byte OID value
 *
 * Valid for values up to 127
 */
#define ASN1_OID_SINGLE( value ) ( (value) & 0x7f )

/** Double-byte OID value
 *
 * Valid for values up to 16383
 */
#define ASN1_OID_DOUBLE( value ) \
	( 0x80 | ( ( (value) >> 7 ) & 0x7f ) ), ASN1_OID_SINGLE ( (value) )

/** Double-byte OID value
 *
 * Valid for values up to 2097151
 */
#define ASN1_OID_TRIPLE( value ) \
	( 0x80 | ( ( (value) >> 14 ) & 0x7f ) ), ASN1_OID_DOUBLE ( (value) )

/** ASN.1 OID for rsaEncryption (1.2.840.113549.1.1.1) */
#define ASN1_OID_RSAENCRYPTION					\
	ASN1_OID_INITIAL ( 1, 2 ), ASN1_OID_DOUBLE ( 840 ),	\
	ASN1_OID_TRIPLE ( 113549 ), ASN1_OID_SINGLE ( 1 ),	\
	ASN1_OID_SINGLE ( 1 ), ASN1_OID_SINGLE ( 1 )

/** ASN.1 OID for md5WithRSAEncryption (1.2.840.113549.1.1.4) */
#define ASN1_OID_MD5WITHRSAENCRYPTION				\
	ASN1_OID_INITIAL ( 1, 2 ), ASN1_OID_DOUBLE ( 840 ),	\
	ASN1_OID_TRIPLE ( 113549 ), ASN1_OID_SINGLE ( 1 ),	\
	ASN1_OID_SINGLE ( 1 ), ASN1_OID_SINGLE ( 4 )

/** ASN.1 OID for sha1WithRSAEncryption (1.2.840.113549.1.1.5) */
#define ASN1_OID_SHA1WITHRSAENCRYPTION				\
	ASN1_OID_INITIAL ( 1, 2 ), ASN1_OID_DOUBLE ( 840 ),	\
	ASN1_OID_TRIPLE ( 113549 ), ASN1_OID_SINGLE ( 1 ),	\
	ASN1_OID_SINGLE ( 1 ), ASN1_OID_SINGLE ( 5 )

/** ASN.1 OID for sha256WithRSAEncryption (1.2.840.113549.1.1.11) */
#define ASN1_OID_SHA256WITHRSAENCRYPTION			\
	ASN1_OID_INITIAL ( 1, 2 ), ASN1_OID_DOUBLE ( 840 ),	\
	ASN1_OID_TRIPLE ( 113549 ), ASN1_OID_SINGLE ( 1 ),	\
	ASN1_OID_SINGLE ( 1 ), ASN1_OID_SINGLE ( 11 )

/** ASN.1 OID for id-md5 (1.2.840.113549.2.5) */
#define ASN1_OID_MD5						\
	ASN1_OID_INITIAL ( 1, 2 ), ASN1_OID_DOUBLE ( 840 ),	\
	ASN1_OID_TRIPLE ( 113549 ), ASN1_OID_SINGLE ( 2 ),	\
	ASN1_OID_SINGLE ( 5 )

/** ASN.1 OID for id-sha1 (1.3.14.3.2.26) */
#define ASN1_OID_SHA1						\
	ASN1_OID_INITIAL ( 1, 3 ), ASN1_OID_SINGLE ( 14 ),	\
	ASN1_OID_SINGLE ( 3 ), ASN1_OID_SINGLE ( 2 ),		\
	ASN1_OID_SINGLE ( 26 )

/** ASN.1 OID for id-sha256 (2.16.840.1.101.3.4.2.1) */
#define ASN1_OID_SHA256						\
	ASN1_OID_INITIAL ( 2, 16 ), ASN1_OID_DOUBLE ( 840 ),	\
	ASN1_OID_SINGLE ( 1 ), ASN1_OID_SINGLE ( 101 ),		\
	ASN1_OID_SINGLE ( 3 ), ASN1_OID_SINGLE ( 4 ),		\
	ASN1_OID_SINGLE ( 2 ), ASN1_OID_SINGLE ( 1 )

/** ASN.1 OID for commonName (2.5.4.3) */
#define ASN1_OID_COMMON_NAME					\
	ASN1_OID_INITIAL ( 2, 5 ), ASN1_OID_SINGLE ( 4 ),	\
	ASN1_OID_SINGLE ( 3 )

/** ASN.1 OID for id-ce-keyUsage (2.5.29.15) */
#define ASN1_OID_KEYUSAGE					\
	ASN1_OID_INITIAL ( 2, 5 ), ASN1_OID_SINGLE ( 29 ),	\
	ASN1_OID_SINGLE ( 15 )

/** ASN.1 OID for id-ce-basicConstraints (2.5.29.19) */
#define ASN1_OID_BASICCONSTRAINTS				\
	ASN1_OID_INITIAL ( 2, 5 ), ASN1_OID_SINGLE ( 29 ),	\
	ASN1_OID_SINGLE ( 19 )

/** Define an ASN.1 cursor containing an OID */
#define ASN1_OID_CURSOR( oid_value ) {				\
		.data = oid_value,				\
		.len = sizeof ( oid_value ),			\
	}

/** An ASN.1 OID-identified algorithm */
struct asn1_algorithm {
	/** Name */
	const char *name;
	/** Object identifier */
	struct asn1_cursor oid;
	/** Public-key algorithm (if applicable) */
	struct pubkey_algorithm *pubkey;
	/** Digest algorithm (if applicable) */
	struct digest_algorithm *digest;
};

/** ASN.1 OID-identified algorithms */
#define ASN1_ALGORITHMS __table ( struct asn1_algorithm, "asn1_algorithms" )

/** Declare an ASN.1 OID-identified algorithm */
#define __asn1_algorithm __table_entry ( ASN1_ALGORITHMS, 01 )

/** An ASN.1 boolean */
struct asn1_boolean {
	/** Value */
	uint8_t value;
} __attribute__ (( packed ));

/** An ASN.1 bit string */
struct asn1_bit_string {
	/** Number of unused bits */
	uint8_t unused;
	/** Data */
	uint8_t data[0];
} __attribute__ (( packed ));

/**
 * Extract ASN.1 type
 *
 * @v cursor		ASN.1 object cursor
 * @ret type		Type
 */
static inline __attribute__ (( always_inline )) unsigned int
asn1_type ( const struct asn1_cursor *cursor ) {
	return ( *( ( const uint8_t * ) cursor->data ) );
}

extern void asn1_invalidate_cursor ( struct asn1_cursor *cursor );
extern int asn1_enter ( struct asn1_cursor *cursor, unsigned int type );
extern int asn1_skip_if_exists ( struct asn1_cursor *cursor,
				 unsigned int type );
extern int asn1_skip ( struct asn1_cursor *cursor, unsigned int type );
extern int asn1_shrink ( struct asn1_cursor *cursor, unsigned int type );
extern int asn1_enter_any ( struct asn1_cursor *cursor );
extern int asn1_skip_any ( struct asn1_cursor *cursor );
extern int asn1_shrink_any ( struct asn1_cursor *cursor );
extern int asn1_boolean ( const struct asn1_cursor *cursor );
extern int asn1_integer ( const struct asn1_cursor *cursor, int *value );
extern int asn1_compare ( const struct asn1_cursor *cursor1,
			  const struct asn1_cursor *cursor2 );
extern struct asn1_algorithm *
asn1_algorithm ( const struct asn1_cursor *cursor );

#endif /* _IPXE_ASN1_H */
