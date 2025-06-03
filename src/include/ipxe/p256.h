#ifndef _IPXE_P256_H
#define _IPXE_P256_H

/** @file
 *
 * NIST P-256 elliptic curve
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/weierstrass.h>

/** P-256 value length */
#define P256_LEN ( 256 / 8 )

extern struct elliptic_curve p256_curve;

#endif /* _IPXE_P256_H */
