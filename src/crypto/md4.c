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
FILE_SECBOOT ( PERMITTED );

/** @file
 *
 * MD4 algorithm
 *
 */

#include <stdint.h>
#include <assert.h>
#include <ipxe/rotate.h>
#include <ipxe/crypto.h>
#include <ipxe/md4.h>

/** MD4 variables */
struct md4_variables {
	/* This layout matches that of struct md4_digest_data */
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
static const struct md4_step md4_steps[4] = {
	/** 0 to 15 */
	{ .f = md4_f_0_15,	.constant = 0x00000000UL },
	/** 16 to 31 */
	{ .f = md4_f_16_31,	.constant = 0x5a827999UL },
	/** 32 to 47 */
	{ .f = md4_f_32_47,	.constant = 0x6ed9eba1UL },
};

/** MD4 initial digest values */
static const struct md4_digest md4_init = {
	.h = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 }
};

/**
 * Calculate MD4 digest of accumulated data
 *
 * @v dd		Digest and data block
 * @v digest		Copy of current digest value
 */
static void md4_compress ( struct md4_digest_data *dd,
			   const struct md4_digest *digest ) {
        union {
		struct md4_digest_data dd;
		struct md4_variables v;
	} *u = container_of ( dd, typeof ( *u ), dd );
	struct md4_variables *v = &u->v;
	const struct md4_step *step;
	uint32_t *a = &v->a;
	uint32_t *b = &v->b;
	uint32_t *c = &v->c;
	uint32_t *d = &v->d;
	uint32_t *w = v->w;
	uint32_t f;
	uint32_t temp;
	unsigned int round;
	unsigned int i;

	/* Sanity checks */
	build_assert ( &u->dd.digest.h[0] == a );
	build_assert ( &u->dd.digest.h[1] == b );
	build_assert ( &u->dd.digest.h[2] == c );
	build_assert ( &u->dd.digest.h[3] == d );
	build_assert ( &u->dd.data.dword[0] == w );
	build_assert ( sizeof ( u->dd ) == sizeof ( u->v ) );

	/* Main loop */
	for ( i = 0 ; i < 48 ; i++ ) {
		round = ( i / 16 );
		step = &md4_steps[round];
		f = step->f ( v, ( i % 16 ) );
		temp = *d;
		*d = *c;
		*c = *b;
		*b = rol32 ( ( *a + f + step->constant ), r[round][ i % 4 ] );
		*a = temp;
		DBGC2 ( &md4_algorithm, "%2d : %08x %08x %08x %08x\n",
			i, *a, *b, *c, *d );
	}

	/* Add chunk to hash */
	for ( i = 0 ; i < 4 ; i++ )
		dd->digest.h[i] += digest->h[i];
}

/** MD4 algorithm */
MDHASH_ALGORITHM ( md4, md4_algorithm, md4_compress, __LITTLE_ENDIAN,
		   struct md4_digest_data, md4_init, MD4_DIGEST_SIZE );
