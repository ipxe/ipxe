#ifndef _IPXE_X509_H
#define _IPXE_X509_H

/** @file
 *
 * X.509 certificates
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <ipxe/asn1.h>

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

/** An X.509 algorithm */
struct x509_algorithm {
	/** Name */
	const char *name;
	/** Object identifier */
	struct asn1_cursor oid;
	/** Public-key algorithm */
	struct pubkey_algorithm *pubkey;
	/** Digest algorithm (if applicable) */
	struct digest_algorithm *digest;
};

/**
 * Test if X.509 algorithm is a signature algorithm
 *
 * @v algorithm		Algorithm
 * @ret is_signature	Algorithm is a signature algorithm
 */
static inline __attribute__ (( always_inline )) int
x509_is_signature_algorithm ( struct x509_algorithm *algorithm ) {
	return ( algorithm->digest != NULL );
}

/** An X.509 bit string */
struct x509_bit_string {
	/** Data */
	const void *data;
	/** Length */
	size_t len;
	/** Unused bits at end of data */
	unsigned int unused;
};

/** An X.509 issuer */
struct x509_issuer {
	/** Raw issuer */
	struct asn1_cursor raw;
};

/** An X.509 time */
struct x509_time {
	/** Seconds since the Epoch */
	time_t time;
};

/** An X.509 certificate validity period */
struct x509_validity {
	/** Not valid before */
	struct x509_time not_before;
	/** Not valid after */
	struct x509_time not_after;
};

/** An X.509 name */
struct x509_name {
	/** Name (not NUL-terminated) */
	const void *data;
	/** Length of name */
	size_t len;
};

/** An X.509 certificate public key */
struct x509_public_key {
	/** Raw public key */
	struct asn1_cursor raw;
	/** Public key algorithm */
	struct x509_algorithm *algorithm;
};

/** An X.509 certificate subject */
struct x509_subject {
	/** Raw subject */
	struct asn1_cursor raw;
	/** Common name */
	struct x509_name name;
	/** Public key information */
	struct x509_public_key public_key;
};

/** An X.509 certificate signature */
struct x509_signature {
	/** Signature algorithm */
	struct x509_algorithm *algorithm;
	/** Signature value */
	struct x509_bit_string value;
};

/** An X.509 certificate basic constraints set */
struct x509_basic_constraints {
	/** Subject is a CA */
	int ca;
	/** Path length */
	unsigned int path_len;
};

/** An X.509 certificate key usage */
struct x509_key_usage {
	/** Key usage extension is present */
	int present;
	/** Usage bits */
	unsigned int bits;
};

/** X.509 certificate key usage bits */
enum x509_key_usage_bits {
	X509_DIGITAL_SIGNATURE = 0x0080,
	X509_NON_REPUDIATION = 0x0040,
	X509_KEY_ENCIPHERMENT = 0x0020,
	X509_DATA_ENCIPHERMENT = 0x0010,
	X509_KEY_AGREEMENT = 0x0008,
	X509_KEY_CERT_SIGN = 0x0004,
	X509_CRL_SIGN = 0x0002,
	X509_ENCIPHER_ONLY = 0x0001,
	X509_DECIPHER_ONLY = 0x8000,
};

/** An X.509 certificate extensions set */
struct x509_extensions {
	/** Basic constraints */
	struct x509_basic_constraints basic;
	/** Key usage */
	struct x509_key_usage usage;
};

/** An X.509 certificate */
struct x509_certificate {
	/** Raw certificate */
	struct asn1_cursor raw;
	/** Version */
	unsigned int version;
	/** Raw tbsCertificate */
	struct asn1_cursor tbs;
	/** Signature algorithm */
	struct x509_algorithm *signature_algorithm;
	/** Issuer */
	struct x509_issuer issuer;
	/** Validity */
	struct x509_validity validity;
	/** Subject */
	struct x509_subject subject;
	/** Signature */
	struct x509_signature signature;
	/** Extensions */
	struct x509_extensions extensions;
};

/** An X.509 extension */
struct x509_extension {
	/** Name */
	const char *name;
	/** Object identifier */
	struct asn1_cursor oid;
	/** Parse extension
	 *
	 * @v cert		X.509 certificate
	 * @v raw		ASN.1 cursor
	 * @ret rc		Return status code
	 */
	int ( * parse ) ( struct x509_certificate *cert,
			  const struct asn1_cursor *raw );
};

/** An X.509 root certificate store */
struct x509_root {
	/** Fingerprint digest algorithm */
	struct digest_algorithm *digest;
	/** Number of certificates */
	unsigned int count;
	/** Certificate fingerprints */
	const void *fingerprints;
};

extern int x509_parse ( struct x509_certificate *cert,
			const void *data, size_t len );
extern int x509_validate ( struct x509_certificate *cert,
			   struct x509_certificate *issuer );
extern void x509_fingerprint ( struct x509_certificate *cert,
			       struct digest_algorithm *digest,
			       void *fingerprint );
extern int x509_validate_root ( struct x509_certificate *cert,
				struct x509_root *root );
extern int x509_validate_chain ( int ( * parse_next )
				 ( struct x509_certificate *cert,
				   void *context ),
				 void *context, struct x509_root *root,
				 struct x509_certificate *first );

#endif /* _IPXE_X509_H */
