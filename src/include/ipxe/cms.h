#ifndef _IPXE_CMS_H
#define _IPXE_CMS_H

/** @file
 *
 * Cryptographic Message Syntax (PKCS #7)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <time.h>
#include <ipxe/asn1.h>
#include <ipxe/crypto.h>
#include <ipxe/x509.h>
#include <ipxe/uaccess.h>

/** CMS signer information */
struct cms_signer_info {
	/** Issuer name */
	struct asn1_cursor issuer;
	/** Serial number */
	struct asn1_cursor serial;
	/** Digest algorithm */
	struct digest_algorithm *digest;
	/** Public-key algorithm */
	struct pubkey_algorithm *pubkey;
	/** Signature */
	const void *signature;
	/** Length of signature */
	size_t signature_len;
};

/** A CMS signature */
struct cms_signature {
	/** Raw certificate list */
	struct asn1_cursor certificates;
	/** Signer information
	 *
	 * We currently use only the first signer information block.
	 */
	struct cms_signer_info info;
};

extern int cms_parse ( struct cms_signature *sig, const void *data,
		       size_t len );
extern int cms_verify ( struct cms_signature *sig, userptr_t data, size_t len,
			const char *name, time_t time, struct x509_root *root );

#endif /* _IPXE_CMS_H */
