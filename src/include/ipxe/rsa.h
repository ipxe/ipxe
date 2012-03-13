#ifndef _IPXE_RSA_H
#define _IPXE_RSA_H

/** @file
 *
 * RSA public-key cryptography
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <ipxe/crypto.h>
#include <ipxe/bigint.h>
#include <ipxe/asn1.h>

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

/** RSA digestAlgorithm sequence contents */
#define RSA_DIGESTALGORITHM_CONTENTS( ... )				\
	ASN1_OID, VA_ARG_COUNT ( __VA_ARGS__ ), __VA_ARGS__,		\
	ASN1_NULL, 0x00

/** RSA digestAlgorithm sequence */
#define RSA_DIGESTALGORITHM( ... )					\
	ASN1_SEQUENCE,							\
	VA_ARG_COUNT ( RSA_DIGESTALGORITHM_CONTENTS ( __VA_ARGS__ ) ),	\
	RSA_DIGESTALGORITHM_CONTENTS ( __VA_ARGS__ )

/** RSA digest prefix */
#define RSA_DIGEST_PREFIX( digest_size )				\
	ASN1_OCTET_STRING, digest_size

/** RSA digestInfo prefix */
#define RSA_DIGESTINFO_PREFIX( digest_size, ... )			\
	ASN1_SEQUENCE,							\
	( VA_ARG_COUNT ( RSA_DIGESTALGORITHM ( __VA_ARGS__ ) ) +	\
	  VA_ARG_COUNT ( RSA_DIGEST_PREFIX ( digest_size ) ) +		\
	  digest_size ),						\
	RSA_DIGESTALGORITHM ( __VA_ARGS__ ),				\
	RSA_DIGEST_PREFIX ( digest_size )

/** An RSA context */
struct rsa_context {
	/** Allocated memory */
	void *dynamic;
	/** Modulus */
	bigint_element_t *modulus0;
	/** Modulus size */
	unsigned int size;
	/** Modulus length */
	size_t max_len;
	/** Exponent */
	bigint_element_t *exponent0;
	/** Exponent size */
	unsigned int exponent_size;
	/** Input buffer */
	bigint_element_t *input0;
	/** Output buffer */
	bigint_element_t *output0;
};

extern struct pubkey_algorithm rsa_algorithm;

#endif /* _IPXE_RSA_H */
