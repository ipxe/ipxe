/*
 * Copyright (C) 2015 Michael Brown <mbrown@fensystems.co.uk>.
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
 * SHA-512 algorithm
 *
 */

#include <stdint.h>
#include <assert.h>
#include <ipxe/rotate.h>
#include <ipxe/crypto.h>
#include <ipxe/sha512.h>

/** SHA-512 variables */
struct sha512_variables {
	/* This layout matches that of struct sha512_digest_data */
	uint64_t a;
	uint64_t b;
	uint64_t c;
	uint64_t d;
	uint64_t e;
	uint64_t f;
	uint64_t g;
	uint64_t h;
	/* We reuse w[0..15] to construct w[16..79] on demand */
	uint64_t w[16];
} __attribute__ (( packed ));

/** SHA-512 constants */
static const uint64_t k[SHA512_ROUNDS] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL,
	0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
	0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL,
	0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
	0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
	0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL,
	0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL,
	0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
	0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL,
	0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL,
	0x92722c851482353bULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
	0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
	0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL,
	0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
	0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL,
	0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL,
	0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
	0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL,
	0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
	0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
	0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

/** SHA-512 initial digest values */
static const struct sha512_digest sha512_init = {
	.h = {
		0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
		0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
		0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
		0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL,
	}
};

/**
 * Calculate SHA-512 digest of accumulated data
 *
 * @v dd		Digest and data block
 * @v digest		Copy of current digest value
 */
void sha512_compress ( struct sha512_digest_data *dd,
		       const struct sha512_digest *digest ) {
        union {
		struct sha512_digest_data dd;
		struct sha512_variables v;
	} *u = container_of ( dd, typeof ( *u ), dd );
	struct sha512_variables *v = &u->v;
	uint64_t *a = &v->a;
	uint64_t *b = &v->b;
	uint64_t *c = &v->c;
	uint64_t *d = &v->d;
	uint64_t *e = &v->e;
	uint64_t *f = &v->f;
	uint64_t *g = &v->g;
	uint64_t *h = &v->h;
	uint64_t *w = v->w;
	uint64_t s0;
	uint64_t s1;
	uint64_t maj;
	uint64_t t1;
	uint64_t t2;
	uint64_t ch;
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
	build_assert ( &u->dd.data.qword[0] == w );
	build_assert ( sizeof ( u->dd ) == sizeof ( u->v ) );

	/* Main loop */
	for ( i = 0 ; i < SHA512_ROUNDS ; i++ ) {
		s0 = ( ror64 ( *a, 28 ) ^ ror64 ( *a, 34 ) ^ ror64 ( *a, 39 ) );
		maj = ( ( *a & *b ) ^ ( *a & *c ) ^ ( *b & *c ) );
		t2 = ( s0 + maj );
		s1 = ( ror64 ( *e, 14 ) ^ ror64 ( *e, 18 ) ^ ror64 ( *e, 41 ) );
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
		s0 = ( ror64 ( w[ ( i - 15 ) % 16 ], 1 ) ^
		       ror64 ( w[ ( i - 15 ) % 16 ], 8 ) ^
		       ( w[ ( i - 15 ) % 16 ] >> 7 ) );
		s1 = ( ror64 ( w[ ( i - 2 ) % 16 ], 19 ) ^
		       ror64 ( w[ ( i - 2 ) % 16 ], 61 ) ^
		       ( w[ ( i - 2 ) % 16 ] >> 6 ) );
		w[ i % 16 ] = ( w[ ( i - 16 ) % 16 ] + s0 +
				w[ ( i - 7 ) % 16 ] + s1 );
		DBGC2 ( &sha512_algorithm, "%2d : %016llx %016llx %016llx "
			"%016llx %016llx %016llx %016llx %016llx\n",
			i, *a, *b, *c, *d, *e, *f, *g, *h );
	}

	/* Add chunk to hash */
	for ( i = 0 ; i < 8 ; i++ )
		dd->digest.h[i] += digest->h[i];
}

/** SHA-512 algorithm */
SHA512_ALGORITHM ( sha512, sha512_algorithm, sha512_init, SHA512_DIGEST_SIZE );
