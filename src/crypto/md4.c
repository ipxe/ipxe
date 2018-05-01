/*
 * Copyright (C) 2017 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * MD4 algorithm
 *
 */

#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <assert.h>
#include <ipxe/rotate.h>
#include <ipxe/crypto.h>
#include <ipxe/asn1.h>
#include <ipxe/md4.h>

/** MD4 variables */
struct md4_variables {
	/* This layout matches that of struct md4_digest_data,
	 * allowing for efficient endianness-conversion,
	 */
	uint32_t a;
	uint32_t b;
	uint32_t c;
	uint32_t d;
	uint32_t w[16];
} __attribute__ (( packed ));

/** MD4 shift amounts */
static const uint8_t r[3][4] = {
	{  3,  7, 11, 19 },
	{  3,  5,  9, 13 },
	{  3,  9, 11, 15 },
};

/**
 * f(b,c,d,w) for steps 0 to 15
 *
 * @v v		MD4 variables
 * @v i		Index within round
 * @ret f	f(b,c,d,w)
 */
static uint32_t md4_f_0_15 ( struct md4_variables *v, unsigned int i ) {
	return ( ( ( v->b & v->c ) | ( ~v->b & v->d ) ) + v->w[i] );
}

/**
 * f(b,c,d,w) for steps 16 to 31
 *
 * @v v		MD4 variables
 * @v i		Index within round
 * @ret f	f(b,c,d,w)
 */
static uint32_t md4_f_16_31 ( struct md4_variables *v, unsigned int i ) {
	return ( ( ( v->b & v->c ) | ( v->b & v->d ) | ( v->c & v->d ) ) +
		 v->w[ ( ( i << 2 ) | ( i >> 2 ) ) % 16 ] );
}

/**
 * f(b,c,d,w) for steps 32 to 47
 *
 * @v v		MD4 variables
 * @v i		Index within round
 * @ret f	f(b,c,d,w)
 */
static uint32_t md4_f_32_47 ( struct md4_variables *v, unsigned int i ) {
	static const uint8_t reverse[16] = {
		0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15
	};
	return ( ( v->b ^ v->c ^ v->d ) + v->w[reverse[i]] );
}

/** An MD4 step function */
struct md4_step {
	/**
	 * Calculate f(b,c,d,w)
	 *
	 * @v v		MD4 variables
	 * @v i		Index within round
	 * @ret f	f(b,c,d,w)
	 */
	uint32_t ( * f ) ( struct md4_variables *v, unsigned int i );
	/** Constant */
	uint32_t constant;
};

/** MD4 steps */
static struct md4_step md4_steps[4] = {
	/** 0 to 15 */
	{ .f = md4_f_0_15,	.constant = 0x00000000UL },
	/** 16 to 31 */
	{ .f = md4_f_16_31,	.constant = 0x5a827999UL },
	/** 32 to 47 */
	{ .f = md4_f_32_47,	.constant = 0x6ed9eba1UL },
};

/**
 * Initialise MD4 algorithm
 *
 * @v ctx		MD4 context
 */
static void md4_init ( void *ctx ) {
	struct md4_context *context = ctx;

	context->ddd.dd.digest.h[0] = cpu_to_le32 ( 0x67452301 );
	context->ddd.dd.digest.h[1] = cpu_to_le32 ( 0xefcdab89 );
	context->ddd.dd.digest.h[2] = cpu_to_le32 ( 0x98badcfe );
	context->ddd.dd.digest.h[3] = cpu_to_le32 ( 0x10325476 );
	context->len = 0;
}

/**
 * Calculate MD4 digest of accumulated data
 *
 * @v context		MD4 context
 */
static void md4_digest ( struct md4_context *context ) {
        union {
		union md4_digest_data_dwords ddd;
		struct md4_variables v;
	} u;
	uint32_t *a = &u.v.a;
	uint32_t *b = &u.v.b;
	uint32_t *c = &u.v.c;
	uint32_t *d = &u.v.d;
	uint32_t *w = u.v.w;
	uint32_t f;
	uint32_t temp;
	struct md4_step *step;
	unsigned int round;
	unsigned int i;

	/* Sanity checks */
	assert ( ( context->len % sizeof ( context->ddd.dd.data ) ) == 0 );
	linker_assert ( &u.ddd.dd.digest.h[0] == a, md4_bad_layout );
	linker_assert ( &u.ddd.dd.digest.h[1] == b, md4_bad_layout );
	linker_assert ( &u.ddd.dd.digest.h[2] == c, md4_bad_layout );
	linker_assert ( &u.ddd.dd.digest.h[3] == d, md4_bad_layout );
	linker_assert ( &u.ddd.dd.data.dword[0] == w, md4_bad_layout );

	DBGC ( context, "MD4 digesting:\n" );
	DBGC_HDA ( context, 0, &context->ddd.dd.digest,
		   sizeof ( context->ddd.dd.digest ) );
	DBGC_HDA ( context, context->len, &context->ddd.dd.data,
		   sizeof ( context->ddd.dd.data ) );

	/* Convert h[0..3] to host-endian, and initialise a, b, c, d,
	 * and x[0..15]
	 */
	for ( i = 0 ; i < ( sizeof ( u.ddd.dword ) /
			    sizeof ( u.ddd.dword[0] ) ) ; i++ ) {
		le32_to_cpus ( &context->ddd.dword[i] );
		u.ddd.dword[i] = context->ddd.dword[i];
	}

	/* Main loop */
	for ( i = 0 ; i < 48 ; i++ ) {
		round = ( i / 16 );
		step = &md4_steps[round];
		f = step->f ( &u.v, ( i % 16 ) );
		temp = *d;
		*d = *c;
		*c = *b;
		*b = rol32 ( ( *a + f + step->constant ), r[round][ i % 4 ] );
		*a = temp;
		DBGC2 ( context, "%2d : %08x %08x %08x %08x\n",
			i, *a, *b, *c, *d );
	}

	/* Add chunk to hash and convert back to little-endian */
	for ( i = 0 ; i < 4 ; i++ ) {
		context->ddd.dd.digest.h[i] =
			cpu_to_le32 ( context->ddd.dd.digest.h[i] +
				      u.ddd.dd.digest.h[i] );
	}

	DBGC ( context, "MD4 digested:\n" );
	DBGC_HDA ( context, 0, &context->ddd.dd.digest,
		   sizeof ( context->ddd.dd.digest ) );
}

/**
 * Accumulate data with MD4 algorithm
 *
 * @v ctx		MD4 context
 * @v data		Data
 * @v len		Length of data
 */
static void md4_update ( void *ctx, const void *data, size_t len ) {
	struct md4_context *context = ctx;
	const uint8_t *byte = data;
	size_t offset;

	/* Accumulate data a byte at a time, performing the digest
	 * whenever we fill the data buffer
	 */
	while ( len-- ) {
		offset = ( context->len % sizeof ( context->ddd.dd.data ) );
		context->ddd.dd.data.byte[offset] = *(byte++);
		context->len++;
		if ( ( context->len % sizeof ( context->ddd.dd.data ) ) == 0 )
			md4_digest ( context );
	}
}

/**
 * Generate MD4 digest
 *
 * @v ctx		MD4 context
 * @v out		Output buffer
 */
static void md4_final ( void *ctx, void *out ) {
	struct md4_context *context = ctx;
	uint64_t len_bits;
	uint8_t pad;

	/* Record length before pre-processing */
	len_bits = cpu_to_le64 ( ( ( uint64_t ) context->len ) * 8 );

	/* Pad with a single "1" bit followed by as many "0" bits as required */
	pad = 0x80;
	do {
		md4_update ( ctx, &pad, sizeof ( pad ) );
		pad = 0x00;
	} while ( ( context->len % sizeof ( context->ddd.dd.data ) ) !=
		  offsetof ( typeof ( context->ddd.dd.data ), final.len ) );

	/* Append length (in bits) */
	md4_update ( ctx, &len_bits, sizeof ( len_bits ) );
	assert ( ( context->len % sizeof ( context->ddd.dd.data ) ) == 0 );

	/* Copy out final digest */
	memcpy ( out, &context->ddd.dd.digest,
		 sizeof ( context->ddd.dd.digest ) );
}

/** MD4 algorithm */
struct digest_algorithm md4_algorithm = {
	.name		= "md4",
	.ctxsize	= sizeof ( struct md4_context ),
	.blocksize	= sizeof ( union md4_block ),
	.digestsize	= sizeof ( struct md4_digest ),
	.init		= md4_init,
	.update		= md4_update,
	.final		= md4_final,
};

/** "md4" object identifier */
static uint8_t oid_md4[] = { ASN1_OID_MD4 };

/** "md4" OID-identified algorithm */
struct asn1_algorithm oid_md4_algorithm __asn1_algorithm = {
	.name = "md4",
	.digest = &md4_algorithm,
	.oid = ASN1_OID_CURSOR ( oid_md4 ),
};
