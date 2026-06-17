#ifndef _IPXE_FFDHE_H
#define _IPXE_FFDHE_H

/** @file
 *
 * Finite Field Diffie-Hellman Ephemeral key exchange
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stddef.h>
#include <byteswap.h>
#include <ipxe/bigint.h>
#include <ipxe/crypto.h>

/** A finite field DHE group */
struct ffdhe_group {
	/** Group name */
	const char *name;
	/** Group constant */
	const uint8_t *constant;
	/** Length of raw scalar values */
	size_t len;
	/** Number of elements in scalar values */
	unsigned int size;
	/** Length of (short) exponents */
	size_t explen;
	/** Number of elements in exponent values */
	unsigned int expsize;
	/** Least significant interesting bits of modulus (big-endian) */
	uint32_t lsb32;
};

extern void ffdhe_public ( struct exchange_algorithm *exchange,
			   const void *private, void *public );
extern int ffdhe_shared ( struct exchange_algorithm *exchange,
			  const void *private, const void *partner,
			  void *shared );
extern int ffdhe_has_params ( struct exchange_algorithm *exchange,
			      const void *modulus, size_t len,
			      const void *generator, size_t generator_len );

/**
 * Check if key exchange algorithm is a finite field DHE group
 *
 * @v exchange		Key exchange algorithm
 * @ret is_ffdhe	Key exchange algorithm is a finite field DHE group
 */
static inline __attribute__ (( always_inline )) int
is_ffdhe ( struct exchange_algorithm *exchange ) {

	return ( exchange->public == ffdhe_public );
}

/** Define a finite field DHE group */
#define FFDHE_GROUP( _name, _exchange, _constant, _bits, _expbits, _lsb ) \
	static struct ffdhe_group _name ## _group = {			  \
		.name = #_name,					  	  \
		.constant = (_constant),				  \
		.len = ( _bits / 8 ),					  \
		.size = bigint_required_size ( _bits / 8 ),		  \
		.explen = ( ( _expbits + 7 ) / 8 ),			  \
		.expsize = bigint_required_size ( ( _expbits + 7 ) / 8 ), \
		.lsb32 = cpu_to_be32 ( _lsb ),				  \
	};								  \
	struct exchange_algorithm _exchange = {				  \
		.name = #_name,						  \
		.privsize = ( ( _expbits + 7 ) / 8 ),			  \
		.pubsize = ( _bits / 8 ),				  \
		.sharedsize = ( _bits / 8 ),				  \
		.public = ffdhe_public,					  \
		.shared = ffdhe_shared,					  \
		.priv = &_name ## _group,				  \
	}

extern struct exchange_algorithm ffdhe2048_algorithm;
extern struct exchange_algorithm ffdhe3072_algorithm;
extern struct exchange_algorithm ffdhe4096_algorithm;
extern struct exchange_algorithm modp2048_algorithm;
extern struct exchange_algorithm modp3072_algorithm;
extern struct exchange_algorithm modp4096_algorithm;

#endif /* _IPXE_FFDHE_H */
