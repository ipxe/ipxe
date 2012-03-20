#ifndef _IPXE_CLIENTCERT_H
#define _IPXE_CLIENTCERT_H

/** @file
 *
 * Client certificate store
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>

/** A client certificate */
struct client_certificate {
	/** Data */
	const void *data;
	/** Length */
	size_t len;
};

/** A client private key */
struct client_private_key {
	/** Data */
	const void *data;
	/** Length */
	size_t len;
};

extern struct client_certificate client_certificate;
extern struct client_private_key client_private_key;

/**
 * Check for presence of a client certificate
 *
 * @ret have_cert	We have a client certificate and private key
 */
static inline int have_client_certificate ( void ) {
	return ( ( client_certificate.len > 0 ) &&
		 ( client_private_key.len > 0 ) );
}

#endif /* _IPXE_CLIENTCERT_H */
