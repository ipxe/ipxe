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

/** ASN.1 OID for iso(1) member-body(2) */
#define ASN1_OID_ISO_MEMBERBODY ASN1_OID_INITIAL ( 1, 2 )

/** ASN.1 OID for iso(1) identified-organization(3) */
#define ASN1_OID_IDENTIFIED_ORGANIZATION ASN1_OID_INITIAL ( 1, 3 )

/** ASN.1 OID for joint-iso-itu-t(2) ds(5) */
#define ASN1_OID_DIRECTORY_SERVICES ASN1_OID_INITIAL ( 2, 5 )

/** ASN.1 OID for joint-iso-itu-t(2) country(16) */
#define ASN1_OID_COUNTRY ASN1_OID_INITIAL ( 2, 16 )

/** ASN.1 OID for iso(1) member-body(2) us(840) */
#define ASN1_OID_ISO_US ASN1_OID_ISO_MEMBERBODY, ASN1_OID_DOUBLE ( 840 )

/** ASN.1 OID for iso(1) member-body(2) us(840) rsadsi(113549) */
#define ASN1_OID_RSADSI ASN1_OID_ISO_US, ASN1_OID_TRIPLE ( 113549 )

/** ASN.1 OID for iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) */
#define ASN1_OID_PKCS ASN1_OID_RSADSI, ASN1_OID_SINGLE ( 1 )

/** ASN.1 OID for iso(1) member-body(2) us(840) rsadsi(113549)
 * digestAlgorithm(2)
 */
#define ASN1_OID_DIGESTALGORITHM ASN1_OID_RSADSI, ASN1_OID_SINGLE ( 2 )

/** ASN.1 OID for iso(1) identified-organization(3) oiw(14) */
#define ASN1_OID_OIW ASN1_OID_IDENTIFIED_ORGANIZATION, ASN1_OID_SINGLE ( 14 )

/** ASN.1 OID for iso(1) identified-organization(3) oiw(14) secsig(3) */
#define ASN1_OID_SECSIG ASN1_OID_OIW, ASN1_OID_SINGLE ( 3 )

/** ASN1. OID for iso(1) identified-organization(3) oiw(14) secsig(3)
 * algorithms(2)
 */
#define ASN1_OID_SECSIG_ALGORITHMS ASN1_OID_SECSIG, ASN1_OID_SINGLE ( 2 )

/** ASN.1 OID for joint-iso-itu-t(2) country(16) us(840) */
#define ASN1_OID_COUNTRY_US ASN1_OID_COUNTRY, ASN1_OID_DOUBLE ( 840 )

/** ASN.1 OID for joint-iso-itu-t(2) country(16) us(840) organization(1) */
#define ASN1_OID_US_ORGANIZATION ASN1_OID_COUNTRY_US, ASN1_OID_SINGLE ( 1 )

/** ASN.1 OID for joint-iso-itu-t(2) country(16) us(840)
 * organization(1) gov(101)
 */
#define ASN1_OID_US_GOV ASN1_OID_US_ORGANIZATION, ASN1_OID_SINGLE ( 101 )

/** ASN.1 OID for joint-iso-itu-t(2) country(16) us(840)
 * organization(1) gov(101) csor(3)
 */
#define ASN1_OID_CSOR ASN1_OID_US_GOV, ASN1_OID_SINGLE ( 3 )

/** ASN.1 OID for joint-iso-itu-t(2) country(16) us(840)
 * organization(1) gov(101) csor(3) nistalgorithm(4)
 */
#define ASN1_OID_NISTALGORITHM ASN1_OID_CSOR, ASN1_OID_SINGLE ( 4 )

/** ASN.1 OID for joint-iso-itu-t(2) country(16) us(840)
 * organization(1) gov(101) csor(3) nistalgorithm(4) hashalgs(2)
 */
#define ASN1_OID_HASHALGS ASN1_OID_NISTALGORITHM, ASN1_OID_SINGLE ( 2 )

/** ASN.1 OID for pkcs-1 */
#define ASN1_OID_PKCS_1 ASN1_OID_PKCS, ASN1_OID_SINGLE ( 1 )

/** ASN.1 OID for rsaEncryption */
#define ASN1_OID_RSAENCRYPTION ASN1_OID_PKCS_1, ASN1_OID_SINGLE ( 1 )

/** ASN.1 OID for md5WithRSAEncryption */
#define ASN1_OID_MD5WITHRSAENCRYPTION ASN1_OID_PKCS_1, ASN1_OID_SINGLE ( 4 )

/** ASN.1 OID for sha1WithRSAEncryption */
#define ASN1_OID_SHA1WITHRSAENCRYPTION ASN1_OID_PKCS_1, ASN1_OID_SINGLE ( 5 )

/** ASN.1 OID for sha256WithRSAEncryption */
#define ASN1_OID_SHA256WITHRSAENCRYPTION ASN1_OID_PKCS_1, ASN1_OID_SINGLE ( 11 )

/** ASN.1 OID for id-md5 */
#define ASN1_OID_MD5 ASN1_OID_DIGESTALGORITHM, ASN1_OID_SINGLE ( 5 )

/** ASN.1 OID for id-sha1 */
#define ASN1_OID_SHA1 ASN1_OID_SECSIG_ALGORITHMS, ASN1_OID_SINGLE ( 26 )

/** ASN.1 OID for id-sha256 */
#define ASN1_OID_SHA256 ASN1_OID_HASHALGS, ASN1_OID_SINGLE ( 1 )

/** ASN.1 OID for joint-iso-itu-t(2) ds(5) attributeType(4) */
#define ASN1_OID_ATTRIBUTE_TYPE \
	ASN1_OID_DIRECTORY_SERVICES, ASN1_OID_SINGLE ( 4 )

/** ASN.1 OID for joint-iso-itu-t(2) ds(5) attributeType(4) commonName(3) */
#define ASN1_OID_COMMON_NAME ASN1_OID_ATTRIBUTE_TYPE, ASN1_OID_SINGLE ( 3 )

/** ASN.1 OID for id-ce */
#define ASN1_OID_CE ASN1_OID_DIRECTORY_SERVICES, ASN1_OID_SINGLE ( 29 )

/** ASN.1 OID for id-ce-keyUsage */
#define ASN1_OID_KEYUSAGE ASN1_OID_CE, ASN1_OID_SINGLE ( 15 )

/** ASN.1 OID for id-ce-basicConstraints */
#define ASN1_OID_BASICCONSTRAINTS ASN1_OID_CE, ASN1_OID_SINGLE ( 19 )

/** Define an ASN.1 cursor containing an OID */
#define ASN1_OID_CURSOR( oid_value ) {			\
		.data = oid_value,			\
		.len = sizeof ( oid_value ),		\
	}

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

#endif /* _IPXE_ASN1_H */
