#ifndef _IPXE_DHE_H
#define _IPXE_DHE_H

/** @file
 *
 * Ephemeral Diffie-Hellman key exchange
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

extern int dhe_key ( const void *modulus, size_t len, const void *generator,
		     size_t generator_len, const void *partner,
		     size_t partner_len, const void *private,
		     size_t private_len, void *public, void *shared );

#endif /* _IPXE_DHE_H */
