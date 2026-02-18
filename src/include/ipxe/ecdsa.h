#ifndef _IPXE_ECDSA_H
#define _IPXE_ECDSA_H

/** @file
 *
 * Elliptic curve digital signature algorithm (ECDSA)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <ipxe/crypto.h>

/** Uncompressed curve point */
#define ECDSA_UNCOMPRESSED 0x04

extern struct pubkey_algorithm ecdsa_algorithm;

#endif /* _IPXE_ECDSA_H */
