/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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
 * DES algorithm
 *
 * DES was not designed to be implemented in software, and therefore
 * contains a large number of bit permutation operations that are
 * essentially free in hardware (requiring only wires, no gates) but
 * expensive in software.
 *
 * Since DES is no longer used as a practical block cipher for large
 * volumes of data, we optimise for code size, and do not attempt to
 * obtain fast throughput.
 *
 * The algorithm is specified in NIST SP 800-67, downloadable from
 * https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-67r2.pdf
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/rotate.h>
#include <ipxe/crypto.h>
#include <ipxe/ecb.h>
#include <ipxe/cbc.h>
#include <ipxe/init.h>
#include <ipxe/des.h>

/**
 * DES shift schedule
 *
 * The DES shift schedule (ordered from round 16 down to round 1) is
 * {1,2,2,2,2,2,2,1,2,2,2,2,2,2,1,1}.  In binary, this may be
 * represented as {1,10,10,10,10,10,10,1,10,10,10,10,10,10,1,1} and
 * concatenated (without padding) to produce a single binary integer
 * 1101010101010110101010101011 (equal to 0x0d556aab in hexadecimal).
 *
 * This integer may then be consumed LSB-first, where a 1 bit
 * indicates a shift and the generation of a round key, and a 0 bit
 * indicates a shift without the generation of a round key.
 */
#define DES_SCHEDULE 0x0d556aab

/**
 * Define an element pair in a DES S-box
 *
 * @v x			Upper element of element pair
 * @v y			Lower element of element pair
 *
 * DES S-box elements are 4-bit values.  We encode two values per
 * byte, ordering the elements so that the six-bit input value may be
 * used directly as a lookup index.
 *
 * Specifically, if the input value is {r1,c3,c2,c1,c0,r0}, where
 * {r1,r0} is the table row index and {c3,c2,c1,c0} is the table
 * column index (as used in the DES specification), then:
 *
 *   - {r1,c3,c2,c1,c0} is the byte index into the table
 *
 *   - (4*r0) is the required bit shift to extract the 4-bit value
 */
#define SBYTE( x, y ) ( ( (y) << 4 ) | (x) )

/**
 * Define a row pair in a DES S-box
 *
 * @v x0..xf		Upper row of row pair
 * @v y0..yf		Lower row of row pair
 */
#define SBOX( x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, xa, xb, xc, xd, xe, xf,  \
	      y0, y1, y2, y3, y4, y5, y6, y7, y8, y9, ya, yb, yc, yd, ye, yf ) \
	SBYTE ( x0, y0 ), SBYTE ( x1, y1 ), SBYTE ( x2, y2 ), SBYTE ( x3, y3 ),\
	SBYTE ( x4, y4 ), SBYTE ( x5, y5 ), SBYTE ( x6, y6 ), SBYTE ( x7, y7 ),\
	SBYTE ( x8, y8 ), SBYTE ( x9, y9 ), SBYTE ( xa, ya ), SBYTE ( xb, yb ),\
	SBYTE ( xc, yc ), SBYTE ( xd, yd ), SBYTE ( xe, ye ), SBYTE ( xf, yf )

/** DES S-boxes S1..S8 */
static const uint8_t des_s[8][32] = { {
	/* S1 */
	SBOX ( 14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7,
		0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8 ),
	SBOX (  4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0,
	       15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13 )
}, {
	/* S2 */
	SBOX ( 15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10,
		3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5 ),
	SBOX (  0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15,
		13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9 )
}, {
	/* S3 */
	SBOX ( 10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8,
	       13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1 ),
	SBOX ( 13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7,
		1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12 )
}, {
	/* S4 */
	SBOX (  7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15,
	       13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9 ),
	SBOX ( 10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4,
		3, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14 )
}, {
	/* S5 */
	SBOX (  2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9,
	       14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6 ),
	SBOX (  4,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14,
	       11,  8, 12,  7,  1, 14,  2, 13,  6, 15,  0,  9, 10,  4,  5,  3 )
}, {
	/* S6 */
	SBOX ( 12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11,
	       10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8 ),
	SBOX (  9, 14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6,
		4,  3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13 )
}, {
	/* S7 */
	SBOX (  4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1,
	       13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6 ),
	SBOX (  1,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2,
		6, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12 )
}, {
	/* S8 */
	SBOX ( 13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7,
		1, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2 ),
	SBOX (  7, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8,
		2,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11 )
} };

/**
 * Define a bit index within permuted choice 2 (PC2)
 *
 * @v bit		Bit index
 *
 * Permuted choice 2 (PC2) is used to select bits from a concatenated
 * pair of 28-bit registers ("C" and "D") as part of the key schedule.
 * We store these as 32-bit registers and so must add 4 to indexes
 * above 28.
 */
#define DES_PC2( x ) ( (x) + ( ( (x) > 28 ) ? 4 : 0 ) )

/**
 * Define six bits of permuted choice 2 (PC2)
 *
 * @v r1:r0		Bits corresponding to S-box row index
 * @v c3:c0		Bits corresponding to S-box column index
 *
 * There are 8 steps within a DES round (one step per S-box).  Each
 * step requires six bits of the round key, corresponding to the S-box
 * input value {r1,c3,c2,c1,c0,r0}, where {r1,r0} is the table row
 * index and {c3,c2,c1,c0} is the table column index.
 *
 * As an optimisation, we store the least significant of the 6 bits in
 * the sign bit of a signed 8-bit value, and the remaining 5 bits in
 * the least significant 5 bits of the 8-bit value.  See the comments
 * in des_sbox() for further details.
 */
#define DES_PC2R( r1, c3, c2, c1, c0, r0 )				\
	DES_PC2 ( r0 ), /* LSB stored in sign bit */			\
	DES_PC2 ( r0 ), /* Unused bit */				\
	DES_PC2 ( r0 ), /* Unused bit */				\
	DES_PC2 ( r1 ),	/* Remaining 5 bits */				\
	DES_PC2 ( c3 ),	/* ... */					\
	DES_PC2 ( c2 ),	/* ... */					\
	DES_PC2 ( c1 ),	/* ... */					\
	DES_PC2 ( c0 ) 	/* ... */

/**
 * A DES systematic permutation generator
 *
 * Many of the permutations used in DES comprise systematic bit
 * patterns.  We generate these permutations at runtime to save on
 * code size.
 */
struct des_generator {
	/** Permutation */
	uint8_t *permutation;
	/** Seed value */
	uint32_t seed;
};

/**
 * Define a DES permutation generator
 *
 * @v PERMUTATION	Permutation
 * @v OFFSET		Fixed input bit offset (0 or 1)
 * @v INV<n>		Input bit index bit <n> should be inverted
 * @v BIT<n>		Source bit for input bit index bit <n>
 * @ret generator	Permutation generator
 */
#define DES_GENERATOR( PERMUTATION, OFFSET, INV5, BIT5, INV4, BIT4,	\
		       INV3, BIT3, INV2, BIT2, INV1, BIT1, INV0, BIT0 )	\
	{								\
		.permutation = (PERMUTATION),				\
		.seed = ( ( (INV0) << 31 ) | ( (BIT0) << 28 ) |		\
			  ( (INV1) << 27 ) | ( (BIT1) << 24 ) |		\
			  ( (INV2) << 23 ) | ( (BIT2) << 20 ) |		\
			  ( (INV3) << 19 ) | ( (BIT3) << 16 ) |		\
			  ( (INV4) << 15 ) | ( (BIT4) << 12 ) |		\
			  ( (INV5) << 11 ) | ( (BIT5) <<  8 ) |		\
			  ( ( uint32_t ) sizeof (PERMUTATION) - 1 ) |	\
			  (OFFSET) ),					\
	}

/** DES permuted choice 1 (PC1) "C" register */
static uint8_t des_pc1c[29];

/** DES permuted choice 1 (PC1) "D" register */
static uint8_t des_pc1d[33];

/** DES permuted choice 2 (PC2) */
static const uint8_t des_pc2[65] = {
	DES_PC2R ( 14, 17, 11, 24,  1,  5 ),
	DES_PC2R (  3, 28, 15,  6, 21, 10 ),
	DES_PC2R ( 23, 19, 12,  4, 26,  8 ),
	DES_PC2R ( 16,  7, 27, 20, 13,  2 ),
	DES_PC2R ( 41, 52, 31, 37, 47, 55 ),
	DES_PC2R ( 30, 40, 51, 45, 33, 48 ),
	DES_PC2R ( 44, 49, 39, 56, 34, 53 ),
	DES_PC2R ( 46, 42, 50, 36, 29, 32 ),
	0 /* terminator */
};

/** DES initial permutation (IP) */
static uint8_t des_ip[65];

/** DES data permutation (P) */
static const uint8_t des_p[33] = {
	16,  7, 20, 21, 29, 12, 28, 17,  1, 15, 23, 26,  5, 18, 31, 10,
	 2,  8, 24, 14, 32, 27,  3,  9, 19, 13, 30,  6, 22, 11,  4, 25,
	 0 /* terminator */
};

/** DES final / inverse initial permutation (FP / IP^-1) */
static uint8_t des_fp[65];

/** DES permutation generators */
static struct des_generator des_generators[] = {

	/* The DES initial permutation transforms the bit index
	 * {x5,x4,x3,x2,x1,x0}+1 into {~x2,~x1,~x0,x4,x3,~x5}+1
	 */
	DES_GENERATOR ( des_ip, 1, 1, 2, 1, 1, 1, 0, 0, 4, 0, 3, 1, 5 ),

	/* The DES final permutation transforms the bit index
	 * {x5,x4,x3,x2,x1,x0}+1 into {~x0,x2,x1,~x5,~x4,~x3}+1
	 *
	 * There is an asymmetry in the DES block diagram for the last
	 * of the 16 rounds, which is functionally equivalent to
	 * performing 16 identical rounds and then swapping the left
	 * and right halves before applying the final permutation.  We
	 * may therefore account for this asymmetry by inverting the
	 * MSB in each bit index, to point to the corresponding bit in
	 * the other half.
	 *
	 * This is equivalent to using a permutation that transforms
	 * {x5,x4,x3,x2,x1,x0}+1 into {x0,x2,x1,~x5,~x4,~x3}+1
	 */
	DES_GENERATOR ( des_fp, 1, 0, 0, 0, 2, 0, 1, 1, 5, 1, 4, 1, 3 ),

	/* The "C" half of DES permuted choice 1 (PC1) transforms the
	 * bit index {x5,x4,x3,x2,x1,x0}+1 into {~x2,~x1,~x0,x5,x4,x3}+1
	 */
	DES_GENERATOR ( des_pc1c, 1, 1, 2, 1, 1, 1, 0, 0, 5, 0, 4, 0, 3 ),

	/* The "D" half of DES permuted choice 1 (PC1) transforms the
	 * bit index {x5,x4,x3,x2,x1,x0}+1 into {~x2,~x1,~x0,~x5,~x4,~x3}+0
	 *
	 * Due to the idosyncratic design choice of using 28-bit
	 * registers in the DES key expansion schedule, the final four
	 * permutation values appear at indices [28:31] instead of
	 * [24:27].  This is adjusted for in @c des_setkey().
	 */
	DES_GENERATOR ( des_pc1d, 0, 1, 2, 1, 1, 1, 0, 1, 5, 1, 4, 1, 3 ),
};

/**
 * Generate DES permutation
 *
 * @v generator		Generator
 */
static __attribute__ (( noinline )) void
des_generate ( struct des_generator *generator ) {
	uint8_t *permutation = generator->permutation;
	uint32_t seed = generator->seed;
	unsigned int index = 0;
	uint8_t accum;
	uint8_t bit;

	/* Generate permutations
	 *
	 * This loop is optimised for code size on a
	 * register-constrained architecture such as i386.
	 */
	do {
		/* Rotate seed to access MSB's bit descriptor */
		seed = ror32 ( seed, 8 );

		/* Initialise accumulator with six flag bits */
		accum = 0xfc;

		/* Accumulate bits until all six flag bits are cleared */
		do {
			/* Extract specified bit from index.  Use a
			 * rotation instead of a shift, since this
			 * will allow the mask to be elided.
			 */
			bit = ror8 ( index, ( seed & 0x07 ) );
			seed = ror32 ( seed, 3 );

			/* Toggle bit if applicable */
			bit ^= seed;
			seed = ror32 ( seed, 1 );

			/* Add bit to accumulator and clear one flag bit */
			accum <<= 1;
			accum |= ( bit & 0x01 );

		} while ( accum & 0x80 );

		/* Add constant offset if applicable */
		accum += ( seed & 0x01 );

		/* Store permutation */
		permutation[index] = accum;

		/* Loop until reaching length (which is always even) */
	} while ( ++index < ( seed & 0xfe ) );
	DBGC2 ( permutation, "DES generated permutation %p:\n", permutation );
	DBGC2_HDA ( permutation, 0, permutation,
		    ( ( seed & 0xfe ) + 1 /* zero terminator */ ) );
}

/**
 * Initialise permutations
 */
static void des_init ( void ) {
	unsigned int i;

	/* Generate all generated permutations */
	for ( i = 0 ; i < ( sizeof ( des_generators ) /
			    sizeof ( des_generators[0] ) ) ; i++ ) {
		des_generate ( &des_generators[i] );
	}
}

/** Initialisation function */
struct init_fn des_init_fn __init_fn ( INIT_NORMAL ) = {
	.name = "des",
	.initialise = des_init,
};

/**
 * Perform bit permutation
 *
 * @v permutation	Bit permutation (zero-terminated)
 * @v in		Input value
 * @v out		Output value
 */
static void des_permute ( const uint8_t *permutation, const uint8_t *in,
			  uint8_t *out ) {
	uint8_t mask = 0x80;
	uint8_t accum = 0;
	unsigned int bit;

	/* Extract individual input bits to construct output value */
	while ( ( bit = *(permutation++) ) ) {
		bit--;
		if ( in[ bit / 8 ] & ( 0x80 >> ( bit % 8 ) ) )
			accum |= mask;
		*out = accum;
		mask = ror8 ( mask, 1 );
		if ( mask == 0x80 ) {
			out++;
			accum = 0;
		}
	}
}

/**
 * Perform DES S-box substitution
 *
 * @v in		32-bit input value (native endian)
 * @v rkey		48-bit round key
 * @ret out		32-bit output value (native endian)
 */
static uint32_t des_sbox ( uint32_t in, const union des_round_key *rkey ) {
	uint32_t out = 0;
	uint32_t lookup;
	int32_t key;
	uint8_t sub;
	unsigned int i;

	/* Perform input expansion, key addition, and S-box substitution */
	for ( i = 0 ; i < 8 ; i++ ) {

		/* Rotate input and output */
		out = rol32 ( out, 4 );
		in = rol32 ( in, 4 );

		/* Extract step key from relevant 6 bits of round key
		 *
		 * The least significant of the 6 bits (corresponding
		 * to bit r0 in the S-box lookup index) is stored in
		 * the sign bit of the step key byte.  It will
		 * therefore be propagated via sign extension to the
		 * MSB of the 32-bit step key.
		 *
		 * The remaining 5 of the 6 bits (corresponding to
		 * bits {r1,c3,c2,c1,c0} in the S-box lookup index)
		 * are stored in the least significant 5 bits of the
		 * step key byte and will end up in the least
		 * significant 5 bits of the 32-bit step key.
		 */
		key = rkey->step[i];

		/* Add step key to input to produce S-box lookup index
		 *
		 * We do not ever perform an explicit expansion of the
		 * input value from 32 to 48 bits.  Instead, we rotate
		 * the 32-bit input value by 4 bits on each step, and
		 * extract the relevant 6 bits.
		 *
		 * The least significant of the 6 bits (corresponding
		 * to bit r0 in the S-box lookup index) is currently
		 * in the MSB of the 32-bit (rotated) input value.
		 *
		 * The remaining 5 of the 6 bits (corresponding to
		 * bits {r1,c3,c2,c1,c0} in the S-box lookup index)
		 * are currently in the least significant 5 bits of
		 * the 32-bit (rotated) input value.
		 *
		 * This aligns with the placement of the bits in the
		 * step key (see above), and we can therefore perform
		 * a single XOR to add the 6-bit step key to the
		 * relevant 6 bits of the input value.
		 */
		lookup = ( in ^ key );

		/* Look up S[i][in ^ key] from S-box
		 *
		 * We have bits {r1,c3,c2,c1,c0} in the least
		 * significant 5 bits of the lookup index, and so can
		 * use the masked lookup index directly as a byte
		 * index into the relevant S-box to extract the byte
		 * containing both {r1,c3,c2,c1,c0,'0'} and
		 * {r1,c3,c2,c1,c0,'1'}.
		 *
		 * We then use the MSB of the 32-bit lookup index to
		 * extract the relevant nibble for the full lookup
		 * index {r1,c3,c2,c1,c0,r0}.
		 */
		sub = des_s[i][ lookup & 0x1f ];
		sub >>= ( ( lookup >> 29 ) & 4 );
		sub &= 0x0f;

		/* Substitute S[i][input ^ key] into output */
		out |= sub;
	}

	return out;
}

/**
 * Perform a single DES round
 *
 * @v block		DES block
 * @v rkey		48-bit round key
 */
static void des_round ( union des_block *block,
			const union des_round_key *rkey ) {
	union des_dword sbox;
	uint32_t left;
	uint32_t right;

	/* Extract left and right halves L[n-1] and R[n-1] */
	left = block->left.dword;
	right = block->right.dword;
	DBGC2 ( block, "DES L=%08x R=%08x K=%08x%08x", be32_to_cpu ( left ),
		be32_to_cpu ( right ), be32_to_cpu ( rkey->dword[0] ),
		be32_to_cpu ( rkey->dword[1] ) );

	/* L[n] = R[n-1] */
	block->left.dword = right;

	/* Calculate Feistel function f(R[n-1], K[n]) */
	sbox.dword = cpu_to_be32 ( des_sbox ( be32_to_cpu ( right ), rkey ) );
	des_permute ( des_p, sbox.byte, block->right.byte );

	/* R[n] = L[n-1] + f(R[n-1], K[n]) */
	block->right.dword ^= left;
	DBGC2 ( block, " => L=%08x R=%08x\n",
		be32_to_cpu ( block->left.dword ),
		be32_to_cpu ( block->right.dword ) );
}

/**
 * Perform all DES rounds
 *
 * @v in		Input DES block
 * @v out		Output DES block
 * @v rkey		Starting 48-bit round key
 * @v offset		Byte offset between round keys
 */
static void des_rounds ( const union des_block *in, union des_block *out,
			 const union des_round_key *rkey,
			 ssize_t offset ) {
	union des_block tmp;
	unsigned int i;

	/* Apply initial permutation */
	des_permute ( des_ip, in->byte, tmp.byte );

	/* Perform all DES rounds, consuming keys in the specified order */
	for ( i = 0 ; i < DES_ROUNDS ; i++ ) {
		des_round ( &tmp, rkey );
		rkey = ( ( ( void * ) rkey ) + offset );
	}

	/* Apply final permutation */
	DBGC ( &tmp, "DES %scrypted %08x%08x => ",
	       ( ( offset > 0 ) ? "en" : "de" ), be32_to_cpu ( in->dword[0] ),
	       be32_to_cpu ( in->dword[1] ) );
	des_permute ( des_fp, tmp.byte, out->byte );
	DBGC ( &tmp, "%08x%08x\n", be32_to_cpu ( out->dword[0] ),
	       be32_to_cpu ( out->dword[1] ) );
}

/**
 * Rotate 28-bit word
 *
 * @v dword		28-bit dword value
 * @ret dword		Rotated 28-bit dword value
 */
static uint32_t des_rol28 ( uint32_t dword ) {
	int32_t sdword;

	/* Convert to native-endian */
	sdword = be32_to_cpu ( dword );

	/* Signed shift right by 4 places to copy bit 31 to bits 27:31 */
	sdword >>= 4;

	/* Rotate left */
	sdword = rol32 ( sdword, 1 );

	/* Shift left by 4 places to restore bit positions */
	sdword <<= 4;

	/* Convert back to big-endian */
	dword = cpu_to_be32 ( sdword );

	return dword;
}

/**
 * Set key
 *
 * @v ctx		Context
 * @v key		Key
 * @v keylen		Key length
 * @ret rc		Return status code
 */
static int des_setkey ( void *ctx, const void *key, size_t keylen ) {
	struct des_context *des = ctx;
	union des_round_key *rkey = des->rkey;
	union des_block reg;
	uint32_t schedule;

	/* Validate key length */
	if ( keylen != DES_BLOCKSIZE )
		return -EINVAL;
	DBGC ( des, "DES %p new key:\n", des );
	DBGC_HDA ( des, 0, key, keylen );

	/* Apply permuted choice 1 */
	des_permute ( des_pc1c, key, reg.c.byte );
	des_permute ( des_pc1d, key, reg.d.byte );
	reg.d.byte[3] <<= 4; /* see comment for @c des_pc1d */
	DBGC2 ( des, "DES %p C[ 0]=%07x D[ 0]=%07x\n",
		des, ( be32_to_cpu ( reg.c.dword ) >> 4 ),
		( be32_to_cpu ( reg.d.dword ) >> 4 ) );

	/* Generate round keys */
	for ( schedule = DES_SCHEDULE ; schedule ; schedule >>= 1 ) {

		/* Shift 28-bit words */
		reg.c.dword = des_rol28 ( reg.c.dword );
		reg.d.dword = des_rol28 ( reg.d.dword );

		/* Skip rounds according to shift schedule */
		if ( ! ( schedule & 1 ) )
			continue;

		/* Apply permuted choice 2 */
		des_permute ( des_pc2, reg.byte, rkey->byte );
		DBGC2 ( des, "DES %p C[%2zd]=%07x D[%2zd]=%07x K[%2zd]="
			"%08x%08x\n", des, ( ( rkey - des->rkey ) + 1 ),
			( be32_to_cpu ( reg.c.dword ) >> 4 ),
			( ( rkey - des->rkey ) + 1 ),
			( be32_to_cpu ( reg.d.dword ) >> 4 ),
			( ( rkey - des->rkey ) + 1 ),
			be32_to_cpu ( rkey->dword[0] ),
			be32_to_cpu ( rkey->dword[1] ) );

		/* Move to next key */
		rkey++;
	}

	/* Sanity check */
	assert ( rkey == &des->rkey[DES_ROUNDS] );

	return 0;
}

/**
 * Encrypt data
 *
 * @v ctx		Context
 * @v src		Data to encrypt
 * @v dst		Buffer for encrypted data
 * @v len		Length of data
 */
static void des_encrypt ( void *ctx, const void *src, void *dst, size_t len ) {
	struct des_context *des = ctx;

	/* Sanity check */
	assert ( len == DES_BLOCKSIZE );

	/* Cipher using keys in forward direction */
	des_rounds ( src, dst, &des->rkey[0], sizeof ( des->rkey[0] ) );
}

/**
 * Decrypt data
 *
 * @v ctx		Context
 * @v src		Data to decrypt
 * @v dst		Buffer for decrypted data
 * @v len		Length of data
 */
static void des_decrypt ( void *ctx, const void *src, void *dst, size_t len ) {
	struct des_context *des = ctx;

	/* Sanity check */
	assert ( len == DES_BLOCKSIZE );

	/* Cipher using keys in reverse direction */
	des_rounds ( src, dst, &des->rkey[ DES_ROUNDS - 1 ],
		     -sizeof ( des->rkey[0] ) );
}

/** Basic DES algorithm */
struct cipher_algorithm des_algorithm = {
	.name = "des",
	.ctxsize = sizeof ( struct des_context ),
	.blocksize = DES_BLOCKSIZE,
	.alignsize = 0,
	.authsize = 0,
	.setkey = des_setkey,
	.setiv = cipher_null_setiv,
	.encrypt = des_encrypt,
	.decrypt = des_decrypt,
	.auth = cipher_null_auth,
};

/* DES in Electronic Codebook mode */
ECB_CIPHER ( des_ecb, des_ecb_algorithm,
	     des_algorithm, struct des_context, DES_BLOCKSIZE );

/* DES in Cipher Block Chaining mode */
CBC_CIPHER ( des_cbc, des_cbc_algorithm,
	     des_algorithm, struct des_context, DES_BLOCKSIZE );
