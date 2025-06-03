#ifndef _IPXE_ECDHE_H
#define _IPXE_ECDHE_H

/** @file
 *
 * Elliptic Curve Ephemeral Diffie-Hellman (ECDHE) key exchange
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/crypto.h>

extern int ecdhe_key ( struct elliptic_curve *curve, const void *partner,
		       const void *private, void *public, void *shared );

#endif /* _IPXE_ECDHE_H */
