#ifndef _IPXE_ASN1_H
#define _IPXE_ASN1_H

/** @file
 *
 * ASN.1 encoding
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** An ASN.1 object cursor */
struct asn1_cursor {
	/** Start of data */
	const void *data;
	/** Length of data */
	size_t len;
};

/** ASN.1 end */
#define ASN1_END 0x00

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
#define ASN1_EXPLICIT_TAG 0xa0

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

/** ASN.1 OID for iso(1) member-body(2) */
#define ASN1_OID_ISO_MEMBERBODY ASN1_OID_INITIAL ( 1, 2 )

/** ASN.1 OID for joint-iso-itu-t(2) ds(5) */
#define ASN1_OID_DIRECTORY_SERVICES ASN1_OID_INITIAL ( 2, 5 )

/** ASN.1 OID for joint-iso-itu-t(2) ds(5) attributeType(4) */
#define ASN1_OID_ATTRIBUTE_TYPE \
	ASN1_OID_DIRECTORY_SERVICES, ASN1_OID_SINGLE ( 4 )

/** ASN.1 OID for joint-iso-itu-t(2) ds(5) attributeType(4) commonName(3) */
#define ASN1_OID_COMMON_NAME ASN1_OID_ATTRIBUTE_TYPE, ASN1_OID_SINGLE ( 3 )

/** Define an ASN.1 cursor containing an OID */
#define ASN1_OID_CURSOR( oid_value ) {			\
		.data = oid_value,			\
		.len = sizeof ( oid_value ),		\
	}

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
extern int asn1_enter_any ( struct asn1_cursor *cursor );
extern int asn1_skip_any ( struct asn1_cursor *cursor );
extern int asn1_compare ( const struct asn1_cursor *cursor1,
			  const struct asn1_cursor *cursor2 );

#endif /* _IPXE_ASN1_H */
