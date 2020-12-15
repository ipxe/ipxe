#ifndef _IPXE_PRIVKEY_H
#define _IPXE_PRIVKEY_H

/** @file
 *
 * Private key
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/asn1.h>
#include <ipxe/refcnt.h>

/** A private key */
struct private_key {
	/** Reference counter */
	struct refcnt refcnt;
	/** ASN.1 object builder */
	struct asn1_builder builder;
};

/**
 * Get reference to private key
 *
 * @v key		Private key
 * @ret key		Private key
 */
static inline __attribute__ (( always_inline )) struct private_key *
privkey_get ( struct private_key *key ) {
	ref_get ( &key->refcnt );
	return key;
}

/**
 * Drop reference to private key
 *
 * @v key		Private key
 */
static inline __attribute__ (( always_inline )) void
privkey_put ( struct private_key *key ) {
	ref_put ( &key->refcnt );
}

/**
 * Get private key ASN.1 cursor
 *
 * @v key		Private key
 * @ret cursor		ASN.1 cursor
 */
static inline __attribute__ (( always_inline )) struct asn1_cursor *
privkey_cursor ( struct private_key *key ) {
	return asn1_built ( &key->builder );
}

extern void privkey_free ( struct refcnt *refcnt );

/**
 * Initialise empty private key
 *
 */
static inline __attribute__ (( always_inline )) void
privkey_init ( struct private_key *key ) {
	ref_init ( &key->refcnt, privkey_free );
}

extern struct private_key private_key;

#endif /* _IPXE_PRIVKEY_H */
