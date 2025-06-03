#ifndef _IPXE_P384_H
#define _IPXE_P384_H

/** @file
 *
 * NIST P-384 elliptic curve
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/weierstrass.h>

/** P-384 value length */
#define P384_LEN ( 384 / 8 )

extern struct elliptic_curve p384_curve;

#endif /* _IPXE_P384_H */
