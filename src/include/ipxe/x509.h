#ifndef _IPXE_X509_H
#define _IPXE_X509_H

/** @file
 *
 * X.509 certificates
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <stdlib.h>
#include <ipxe/asn1.h>

/** An X.509 RSA public key */
struct x509_rsa_public_key {
	/** Raw public key */
	struct asn1_cursor raw;
};

extern int x509_rsa_public_key ( const struct asn1_cursor *certificate,
				 struct x509_rsa_public_key *rsa_pubkey );

#endif /* _IPXE_X509_H */
