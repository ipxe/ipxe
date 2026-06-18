#ifndef _IPXE_ELLIPTIC_H
#define _IPXE_ELLIPTIC_H

/** @file
 *
 * Elliptic curves
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <ipxe/crypto.h>

/** An uncompressed elliptic curve point */
#define elliptic_uncompressed_t( len )					\
	struct {							\
		uint8_t format;						\
		union {							\
			struct {					\
				uint8_t x[ (len) ];			\
				uint8_t y[ (len) ];			\
			} __attribute__ (( packed ));			\
			uint8_t xy[ (len) * 2 ];			\
		};							\
	} __attribute__ (( packed ))

/** Format byte for uncompressed curve point representation */
#define ELLIPTIC_FORMAT_UNCOMPRESSED 0x04

extern void elliptic_share ( struct exchange_algorithm *exchange,
			     const void *private, void *public );
extern int elliptic_agree ( struct exchange_algorithm *exchange,
			    const void *private, const void *partner,
			    void *shared );

/** Define an uncompressed elliptic curve point key exchange algorithm */
#define ELLIPTIC_EXCHANGE( _name, _exchange, _len, _curve )		\
	struct exchange_algorithm _exchange = {				\
		.name = #_name,						\
		.privsize = (_len),					\
		.pubsize = sizeof ( elliptic_uncompressed_t (_len) ),	\
		.sharedsize = (_len),					\
		.share = elliptic_share,				\
		.agree = elliptic_agree,				\
		.priv = _curve,						\
	}

#endif /* _IPXE_ELLIPTIC_H */
