/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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
FILE_SECBOOT ( PERMITTED );

/** @file
 *
 * SHA-1 algorithm
 *
 */

#include <stdint.h>
#include <assert.h>
#include <ipxe/rotate.h>
#include <ipxe/crypto.h>
#include <ipxe/sha1.h>

/** SHA-1 variables */
struct sha1_variables {
	/* This layout matches that of struct sha1_digest_data */
	uint32_t a;
	uint32_t b;
	uint32_t c;
	uint32_t d;
	uint32_t e;
	/* We reuse w[0..15] to construct w[16..79] on demand */
	uint32_t w[16];
} __attribute__ (( packed ));

/**
 * f(a,b,c,d) for steps 0 to 19
 *
 * @v v		SHA-1 variables
 * @ret f	f(a,b,c,d)
 */
static uint32_t sha1_f_0_19 ( struct sha1_variables *v ) {
	return ( ( v->b & v->c ) | ( (~v->b) & v->d ) );
}

/**
 * f(a,b,c,d) for steps 20 to 39 and 60 to 79
 *
 * @v v		SHA-1 variables
 * @ret f	f(a,b,c,d)
 */
static uint32_t sha1_f_20_39_60_79 ( struct sha1_variables *v ) {
	return ( v->b ^ v->c ^ v->d );
}

/**
 * f(a,b,c,d) for steps 40 to 59
 *
 * @v v		SHA-1 variables
 * @ret f	f(a,b,c,d)
 */
static uint32_t sha1_f_40_59 ( struct sha1_variables *v ) {
	return ( ( v->b & v->c ) | ( v->b & v->d ) | ( v->c & v->d ) );
}

/** An SHA-1 step function */
struct sha1_step {
	/**
	 * Calculate f(a,b,c,d)
	 *
	 * @v v		SHA-1 variables
	 * @ret f	f(a,b,c,d)
	 */
	uint32_t ( * f ) ( struct sha1_variables *v );
	/** Constant k */
	uint32_t k;
};

/** SHA-1 steps */
static const struct sha1_step sha1_steps[4] = {
	/** 0 to 19 */
	{ .f = sha1_f_0_19,		.k = 0x5a827999 },
	/** 20 to 39 */
	{ .f = sha1_f_20_39_60_79,	.k = 0x6ed9eba1 },
	/** 40 to 59 */
	{ .f = sha1_f_40_59,		.k = 0x8f1bbcdc },
	/** 60 to 79 */
	{ .f = sha1_f_20_39_60_79,	.k = 0xca62c1d6 },
};

/** SHA-1 initial digest values */
static const struct sha1_digest sha1_init = {
	.h = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 }
};

/**
 * Calculate SHA-1 digest of accumulated data
 *
 * @v dd		Digest and data block
 * @v digest		Copy of current digest value
 */
static void sha1_compress ( struct sha1_digest_data *dd,
			    const struct sha1_digest *digest ) {
        union {
		struct sha1_digest_data dd;
		struct sha1_variables v;
	} *u = container_of ( dd, typeof ( *u ), dd );
	struct sha1_variables *v = &u->v;
	const struct sha1_step *step;
	uint32_t *a = &v->a;
	uint32_t *b = &v->b;
	uint32_t *c = &v->c;
	uint32_t *d = &v->d;
	uint32_t *e = &v->e;
	uint32_t *w = v->w;
	uint32_t f;
	uint32_t k;
	uint32_t temp;
	unsigned int i;

	/* Sanity checks */
	build_assert ( &u->dd.digest.h[0] == a );
	build_assert ( &u->dd.digest.h[1] == b );
	build_assert ( &u->dd.digest.h[2] == c );
	build_assert ( &u->dd.digest.h[3] == d );
	build_assert ( &u->dd.digest.h[4] == e );
	build_assert ( &u->dd.data.dword[0] == w );
	build_assert ( sizeof ( u->dd ) == sizeof ( u->v ) );

	/* Main loop */
	for ( i = 0 ; i < 80 ; i++ ) {
		step = &sha1_steps[ i / 20 ];
		f = step->f ( v );
		k = step->k;
		temp = ( rol32 ( *a, 5 ) + f + *e + k + w[ i % 16 ] );
		*e = *d;
		*d = *c;
		*c = rol32 ( *b, 30 );
		*b = *a;
		*a = temp;
		w[ i % 16 ] = rol32 ( ( w[ ( i - 3 ) % 16 ] ^
					w[ ( i - 8 ) % 16 ] ^
					w[ ( i - 14 ) % 16 ] ^
					w[ ( i - 16 ) % 16 ] ), 1 );
		DBGC2 ( &sha1_algorithm, "%2d : %08x %08x %08x %08x %08x\n",
			i, *a, *b, *c, *d, *e );
	}

	/* Add chunk to hash */
	for ( i = 0 ; i < 5 ; i++ )
		dd->digest.h[i] += digest->h[i];
}

/** SHA-1 algorithm */
MDHASH_ALGORITHM ( sha1, sha1_algorithm, sha1_compress, __BIG_ENDIAN,
		   struct sha1_digest_data, sha1_init, SHA1_DIGEST_SIZE );
