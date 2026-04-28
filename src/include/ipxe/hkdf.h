#ifndef _IPXE_HKDF_H
#define _IPXE_HKDF_H

/** @file
 *
 * HMAC-based Extract-and-Expand Key Derivation Function (HKDF)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <ipxe/crypto.h>

extern void hkdf_extract ( struct digest_algorithm *digest, const void *salt,
			   size_t salt_len, const void *ikm, size_t ikm_len,
			   void *prk );
extern void hkdf_expand ( struct digest_algorithm *digest, const void *prk,
			  const void *info, size_t info_len, void *out,
			  size_t len );

#endif /* _IPXE_HKDF_H */
