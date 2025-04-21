#ifndef _IPXE_CMS_H
#define _IPXE_CMS_H

/** @file
 *
 * Cryptographic Message Syntax (PKCS #7)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <time.h>
#include <ipxe/asn1.h>
#include <ipxe/crypto.h>
#include <ipxe/x509.h>
#include <ipxe/refcnt.h>

struct image;
struct cms_message;

/** A CMS message type */
struct cms_type {
	/** Name */
	const char *name;
	/** Object identifier */
	struct asn1_cursor oid;
	/** Parse content
	 *
	 * @v cms		CMS message
	 * @v raw		ASN.1 cursor
	 * @ret rc		Return status code
	 */
	int ( * parse ) ( struct cms_message *cms,
			  const struct asn1_cursor *raw );
};

/** CMS participant information */
struct cms_participant {
	/** List of participant information blocks */
	struct list_head list;
	/** Certificate chain */
	struct x509_chain *chain;

	/** Digest algorithm (for signature messages) */
	struct digest_algorithm *digest;
	/** Public-key algorithm */
	struct pubkey_algorithm *pubkey;

	/** Signature or key value */
	struct asn1_cursor value;
};

/** A CMS message */
struct cms_message {
	/** Reference count */
	struct refcnt refcnt;
	/** Raw ASN.1 data */
	struct asn1_cursor *raw;
	/** Message type */
	struct cms_type *type;

	/** List of all certificates (for signature messages) */
	struct x509_chain *certificates;
	/** List of participant information blocks */
	struct list_head participants;

	/** Cipher algorithm */
	struct cipher_algorithm *cipher;
	/** Cipher initialization vector */
	struct asn1_cursor iv;
	/** Cipher authentication tag */
	struct asn1_cursor mac;
};

/**
 * Get reference to CMS message
 *
 * @v cms		CMS message
 * @ret cms		CMS message
 */
static inline __attribute__ (( always_inline )) struct cms_message *
cms_get ( struct cms_message *cms ) {
	ref_get ( &cms->refcnt );
	return cms;
}

/**
 * Drop reference to CMS message
 *
 * @v cms		CMS message
 */
static inline __attribute__ (( always_inline )) void
cms_put ( struct cms_message *cms ) {
	ref_put ( &cms->refcnt );
}

/**
 * Check if CMS message is a signature message
 *
 * @v cms		CMS message
 * @ret is_signature	Message is a signature message
 */
static inline __attribute__ (( always_inline )) int
cms_is_signature ( struct cms_message *cms ) {

	/* CMS signatures include an optional CertificateSet */
	return ( cms->certificates != NULL );
}

/**
 * Check if CMS message is an encrypted message
 *
 * @v cms		CMS message
 * @ret is_encrypted	Message is an encrypted message
 */
static inline __attribute__ (( always_inline )) int
cms_is_encrypted ( struct cms_message *cms ) {

	/* CMS encrypted messages have a cipher algorithm */
	return ( cms->cipher != NULL );
}

extern int cms_message ( struct image *image, struct cms_message **cms );
extern int cms_verify ( struct cms_message *cms, struct image *image,
			const char *name, time_t time, struct x509_chain *store,
			struct x509_root *root );
extern int cms_decrypt ( struct cms_message *cms, struct image *image,
			 const char *name, struct private_key *private_key );

#endif /* _IPXE_CMS_H */
