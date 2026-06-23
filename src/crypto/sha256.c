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
 * SHA-256 algorithm
 *
 */

#include <stdint.h>
#include <assert.h>
#include <ipxe/rotate.h>
#include <ipxe/crypto.h>
#include <ipxe/sha256.h>

/** SHA-256 variables */
struct sha256_variables {
	/* This layout matches that of struct sha256_digest_data */
	uint32_t a;
	uint32_t b;
	uint32_t c;
	uint32_t d;
	uint32_t e;
	uint32_t f;
	uint32_t g;
	uint32_t h;
	/* We reuse w[0..15] to construct w[16..63] on demand */
	uint32_t w[16];
} __attribute__ (( packed ));

/** SHA-256 constants */
static const uint32_t k[SHA256_ROUNDS] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
	0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
	0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
	0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
	0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
	0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/** SHA-256 initial digest values */
static const struct sha256_digest sha256_init = {
	.h = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
	}
};

/**
 * Calculate SHA-256 digest of accumulated data
 *
 * @v dd		Digest and data block
 * @v digest		Copy of current digest value
 */
void sha256_compress ( struct sha256_digest_data *dd,
		       const struct sha256_digest *digest ) {
        union {
		struct sha256_digest_data dd;
		struct sha256_variables v;
	} *u = container_of ( dd, typeof ( *u ), dd );
	struct sha256_variables *v = &u->v;
	uint32_t *a = &v->a;
	uint32_t *b = &v->b;
	uint32_t *c = &v->c;
	uint32_t *d = &v->d;
	uint32_t *e = &v->e;
	uint32_t *f = &v->f;
	uint32_t *g = &v->g;
	uint32_t *h = &v->h;
	uint32_t *w = v->w;
	uint32_t s0;
	uint32_t s1;
	uint32_t maj;
	uint32_t t1;
	uint32_t t2;
	uint32_t ch;
	unsigned int i;

	/* Sanity checks */
	build_assert ( &u->dd.digest.h[0] == a );
	build_assert ( &u->dd.digest.h[1] == b );
	build_assert ( &u->dd.digest.h[2] == c );
	build_assert ( &u->dd.digest.h[3] == d );
	build_assert ( &u->dd.digest.h[4] == e );
	build_assert ( &u->dd.digest.h[5] == f );
	build_assert ( &u->dd.digest.h[6] == g );
	build_assert ( &u->dd.digest.h[7] == h );
	build_assert ( &u->dd.data.dword[0] == w );
	build_assert ( sizeof ( u->dd ) == sizeof ( u->v ) );

	/* Main loop */
	for ( i = 0 ; i < SHA256_ROUNDS ; i++ ) {
		s0 = ( ror32 ( *a, 2 ) ^ ror32 ( *a, 13 ) ^ ror32 ( *a, 22 ) );
		maj = ( ( *a & *b ) ^ ( *a & *c ) ^ ( *b & *c ) );
		t2 = ( s0 + maj );
		s1 = ( ror32 ( *e, 6 ) ^ ror32 ( *e, 11 ) ^ ror32 ( *e, 25 ) );
		ch = ( ( *e & *f ) ^ ( (~*e) & *g ) );
		t1 = ( *h + s1 + ch + k[i] + w[ i % 16 ] );
		*h = *g;
		*g = *f;
		*f = *e;
		*e = ( *d + t1 );
		*d = *c;
		*c = *b;
		*b = *a;
		*a = ( t1 + t2 );
		s0 = ( ror32 ( w[ ( i - 15 ) % 16 ], 7 ) ^
		       ror32 ( w[ ( i - 15 ) % 16 ], 18 ) ^
		       ( w[ ( i - 15 ) % 16 ] >> 3 ) );
		s1 = ( ror32 ( w[ ( i - 2 ) % 16 ], 17 ) ^
		       ror32 ( w[ ( i - 2 ) % 16 ], 19 ) ^
		       ( w[ ( i - 2 ) % 16 ] >> 10 ) );
		w[ i % 16 ] = ( w[ ( i - 16 ) % 16 ] + s0 +
				w[ ( i - 7 ) % 16 ] + s1 );
		DBGC2 ( &sha256_algorithm,
			"%2d : %08x %08x %08x %08x %08x %08x %08x %08x\n",
			i, *a, *b, *c, *d, *e, *f, *g, *h );
	}

	/* Add chunk to hash */
	for ( i = 0 ; i < 8 ; i++ )
		dd->digest.h[i] += digest->h[i];
}

/** SHA-256 algorithm */
SHA256_ALGORITHM ( sha256, sha256_algorithm, sha256_init, SHA256_DIGEST_SIZE );
