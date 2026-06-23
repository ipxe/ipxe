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
 * MD5 algorithm
 *
 */

#include <stdint.h>
#include <assert.h>
#include <ipxe/rotate.h>
#include <ipxe/crypto.h>
#include <ipxe/md5.h>

/** MD5 variables */
struct md5_variables {
	/* This layout matches that of struct md5_digest_data */
	uint32_t a;
	uint32_t b;
	uint32_t c;
	uint32_t d;
	uint32_t w[16];
} __attribute__ (( packed ));

/** MD5 constants */
static const uint32_t k[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
	0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
	0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
	0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
	0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
	0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

/** MD5 shift amounts */
static const uint8_t r[4][4] = {
	{  7, 12, 17, 22 },
	{  5,  9, 14, 20 },
	{  4, 11, 16, 23 },
	{  6, 10, 15, 21 },
};

/**
 * f(b,c,d) for steps 0 to 15
 *
 * @v v		MD5 variables
 * @ret f	f(b,c,d)
 */
static uint32_t md5_f_0_15 ( struct md5_variables *v ) {
	return ( v->d ^ ( v->b & ( v->c ^ v->d ) ) );
}

/**
 * f(b,c,d) for steps 16 to 31
 *
 * @v v		MD5 variables
 * @ret f	f(b,c,d)
 */
static uint32_t md5_f_16_31 ( struct md5_variables *v ) {
	return ( v->c ^ ( v->d & ( v->b ^ v->c ) ) );
}

/**
 * f(b,c,d) for steps 32 to 47
 *
 * @v v		MD5 variables
 * @ret f	f(b,c,d)
 */
static uint32_t md5_f_32_47 ( struct md5_variables *v ) {
	return ( v->b ^ v->c ^ v->d );
}

/**
 * f(b,c,d) for steps 48 to 63
 *
 * @v v		MD5 variables
 * @ret f	f(b,c,d)
 */
static uint32_t md5_f_48_63 ( struct md5_variables *v ) {
	return ( v->c ^ ( v->b | (~v->d) ) );
}

/** An MD5 step function */
struct md5_step {
	/**
	 * Calculate f(b,c,d)
	 *
	 * @v v		MD5 variables
	 * @ret f	f(b,c,d)
	 */
	uint32_t ( * f ) ( struct md5_variables *v );
	/** Coefficient of i in g=ni+m */
	uint8_t coefficient;
	/** Constant term in g=ni+m */
	uint8_t constant;
};

/** MD5 steps */
static const struct md5_step md5_steps[4] = {
	/** 0 to 15 */
	{ .f = md5_f_0_15,	.coefficient = 1,	.constant = 0 },
	/** 16 to 31 */
	{ .f = md5_f_16_31,	.coefficient = 5,	.constant = 1 },
	/** 32 to 47 */
	{ .f = md5_f_32_47,	.coefficient = 3,	.constant = 5 },
	/** 48 to 63 */
	{ .f = md5_f_48_63,	.coefficient = 7,	.constant = 0 },
};

/** MD5 initial digest values */
static const struct md5_digest md5_init = {
	.h = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 }
};

/**
 * Calculate MD5 digest of accumulated data
 *
 * @v dd		Digest and data block
 * @v digest		Copy of current digest value
 */
static void md5_compress ( struct md5_digest_data *dd,
			   const struct md5_digest *digest ) {
        union {
		struct md5_digest_data dd;
		struct md5_variables v;
	} *u = container_of ( dd, typeof ( *u ), dd );
	struct md5_variables *v = &u->v;
	const struct md5_step *step;
	uint32_t *a = &v->a;
	uint32_t *b = &v->b;
	uint32_t *c = &v->c;
	uint32_t *d = &v->d;
	uint32_t *w = v->w;
	uint32_t f;
	uint32_t g;
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
	for ( i = 0 ; i < 64 ; i++ ) {
		round = ( i / 16 );
		step = &md5_steps[round];
		f = step->f ( v );
		g = ( ( ( step->coefficient * i ) + step->constant ) % 16 );
		temp = *d;
		*d = *c;
		*c = *b;
		*b = ( *b + rol32 ( ( *a + f + k[i] + w[g] ),
				    r[round][ i % 4 ] ) );
		*a = temp;
		DBGC2 ( &md5_algorithm, "%2d : %08x %08x %08x %08x\n",
			i, *a, *b, *c, *d );
	}

	/* Add chunk to hash */
	for ( i = 0 ; i < 4 ; i++ )
		dd->digest.h[i] += digest->h[i];
}

/** MD5 algorithm */
MDHASH_ALGORITHM ( md5, md5_algorithm, md5_compress, __LITTLE_ENDIAN,
		   struct md5_digest_data, md5_init, MD5_DIGEST_SIZE );
