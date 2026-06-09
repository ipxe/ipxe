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

extern int ffdhe ( struct ffdhe_group *group, const void *public,
		   const void *private, void *shared );

/** Define a finite field DHE group */
#define FFDHE_GROUP( _name, _exchange, _bits, _expbits, _lsb )		  \
	static struct ffdhe_group _name ## _group = {			  \
		.name = #_name,					  	  \
		.len = ( _bits / 8 ),					  \
		.size = bigint_required_size ( _bits / 8 ),		  \
		.explen = ( ( _expbits + 7 ) / 8 ),			  \
		.expsize = bigint_required_size ( ( _expbits + 7 ) / 8 ), \
		.lsb32 = cpu_to_be32 ( _lsb ),				  \
	};								  \
	static void _name ## _public ( const void *private,		  \
				       void *public ) {			  \
		ffdhe ( &_name ## _group, NULL, private, public );	  \
	}								  \
	static int _name ## _shared ( const void *private,		  \
				      const void *partner,		  \
				      void *shared ) {			  \
		return ffdhe ( &_name ## _group, partner, private,	  \
			       shared );				  \
	}								  \
	struct exchange_algorithm _exchange = {				  \
		.name = #_name,						  \
		.privsize = ( ( _expbits + 7 ) / 8 ),			  \
		.pubsize = ( _bits / 8 ),				  \
		.sharedsize = ( _bits / 8 ),				  \
		.public = _name ## _public,				  \
		.shared = _name ## _shared,				  \
	}

extern struct exchange_algorithm ffdhe2048_algorithm;
extern struct exchange_algorithm ffdhe3072_algorithm;
extern struct exchange_algorithm ffdhe4096_algorithm;

#endif /* _IPXE_FFDHE_H */
