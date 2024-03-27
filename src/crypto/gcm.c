/*
 * Copyright (C) 2022 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Galois/Counter Mode (GCM)
 *
 * The GCM algorithm is specified in
 *
 * https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38d.pdf
 * https://csrc.nist.rip/groups/ST/toolkit/BCM/documents/proposedmodes/gcm/gcm-spec.pdf
 *
 */

#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <ipxe/crypto.h>
#include <ipxe/gcm.h>

/**
 * Perform encryption
 *
 * This value is chosen to allow for ANDing with a fragment length.
 */
#define GCM_FL_ENCRYPT 0x00ff

/**
 * Calculate hash over an initialisation vector value
 *
 * The hash calculation for a non 96-bit initialisation vector is
 * identical to the calculation used for additional data, except that
 * the non-additional data length counter is used.
 */
#define GCM_FL_IV 0x0100

/**
 * GCM field polynomial
 *
 * GCM treats 128-bit blocks as polynomials in GF(2^128) with the
 * field polynomial f(x) = 1 + x + x^2 + x^7 + x^128.
 *
 * In a somewhat bloody-minded interpretation of "big-endian", the
 * constant term (with degree zero) is arbitrarily placed in the
 * leftmost bit of the big-endian binary representation (i.e. the most
 * significant bit of byte 0), thereby failing to correspond to the
 * bit ordering in any CPU architecture in existence.  This
 * necessitates some wholly gratuitous byte reversals when
 * constructing the multiplication tables, since all CPUs will treat
 * bit 0 as being the least significant bit within a byte.
 *
 * The field polynomial maps to the 128-bit constant
 * 0xe1000000000000000000000000000000 (with the x^128 term outside the
 * 128-bit range), and can therefore be treated as a single-byte
 * value.
 */
#define GCM_POLY 0xe1

/**
 * Hash key for which multiplication tables are cached
 *
 * GCM operates much more efficiently with a cached multiplication
 * table, which costs 4kB per hash key.  Since this exceeds the
 * available stack space, we place a single 4kB cache in .bss and
 * recalculate the cached values as required.  In the common case of a
 * single HTTPS connection being used to download a (relatively) large
 * file, the same key will be used repeatedly for almost all GCM
 * operations, and so the overhead of recalculation is negligible.
 */
static const union gcm_block *gcm_cached_key;

/**
 * Cached multiplication table (M0) for Shoup's method
 *
 * Each entry within this table represents the result of multiplying
 * the cached hash key by an arbitrary 8-bit polynomial.
 */
static union gcm_block gcm_cached_mult[256];

/**
 * Cached reduction table (R) for Shoup's method
 *
 * Each entry within this table represents the result of multiplying
 * the fixed polynomial x^128 by an arbitrary 8-bit polynomial.  Only
 * the leftmost 16 bits are stored, since all other bits within the
 * result will always be zero.
 */
static uint16_t gcm_cached_reduce[256];

/** Offset of a field within GCM context */
#define gcm_offset( field ) offsetof ( struct gcm_context, field )

/**
 * Reverse bits in a byte
 *
 * @v byte		Byte
 * @ret etyb		Bit-reversed byte
 */
static inline __attribute__ (( always_inline )) uint8_t
gcm_reverse ( const uint8_t byte ) {
	uint8_t etyb = etyb;
	uint8_t mask;

	for ( mask = 1 ; mask ; mask <<= 1 ) {
		etyb <<= 1;
		if ( byte & mask )
			etyb |= 1;
	}
	return etyb;
}

/**
 * Update GCM counter
 *
 * @v ctr		Counter
 * @v delta		Amount to add to counter
 */
static inline __attribute__ (( always_inline )) void
gcm_count ( union gcm_block *ctr, uint32_t delta ) {
	uint32_t *value = &ctr->ctr.value;

	/* Update counter modulo 2^32 */
	*value = cpu_to_be32 ( be32_to_cpu ( *value ) + delta );
}

/**
 * XOR partial data block
 *
 * @v src1		Source buffer 1
 * @v src2		Source buffer 2
 * @v dst		Destination buffer
 * @v len		Length
 */
static inline void gcm_xor ( const void *src1, const void *src2, void *dst,
			     size_t len ) {
	uint8_t *dst_bytes = dst;
	const uint8_t *src1_bytes = src1;
	const uint8_t *src2_bytes = src2;

	/* XOR one byte at a time */
	while ( len-- )
		*(dst_bytes++) = ( *(src1_bytes++) ^ *(src2_bytes++) );
}

/**
 * XOR whole data block in situ
 *
 * @v src		Source block
 * @v dst		Destination block
 */
static inline void gcm_xor_block ( const union gcm_block *src,
				   union gcm_block *dst ) {

	/* XOR whole dwords */
	dst->dword[0] ^= src->dword[0];
	dst->dword[1] ^= src->dword[1];
	dst->dword[2] ^= src->dword[2];
	dst->dword[3] ^= src->dword[3];
}

/**
 * Multiply polynomial by (x)
 *
 * @v mult		Multiplicand
 * @v res		Result
 */
static void gcm_multiply_x ( const union gcm_block *mult,
			     union gcm_block *res ) {
	unsigned int i;
	uint8_t byte;
	uint8_t carry;

	/* Multiply by (x) by shifting all bits rightward */
	for ( i = 0, carry = 0 ; i < sizeof ( res->byte ) ; i++ ) {
		byte = mult->byte[i];
		res->byte[i] = ( ( carry << 7 ) | ( byte >> 1 ) );
		carry = ( byte & 0x01 );
	}

	/* If result overflows, reduce modulo the field polynomial */
	if ( carry )
		res->byte[0] ^= GCM_POLY;
}

/**
 * Construct cached tables
 *
 * @v key		Hash key
 * @v context		Context
 */
static void gcm_cache ( const union gcm_block *key ) {
	union gcm_block *mult;
	uint16_t reduce;
	unsigned int this;
	unsigned int other;
	unsigned int i;

	/* Calculate M0[1..255] and R[1..255]
	 *
	 * The R[] values are independent of the key, but the overhead
	 * of recalculating them here is negligible and saves on
	 * overall code size since the calculations are related.
	 */
	for ( i = 1 ; i < 256 ; i++ ) {

		/* Reverse bit order to compensate for poor life choices */
		this = gcm_reverse ( i );

		/* Construct entries */
		mult = &gcm_cached_mult[this];
		if ( this & 0x80 ) {

			/* Odd number: entry[i] = entry[i - 1] + poly */
			other = ( this & 0x7f ); /* bit-reversed (i - 1) */
			gcm_xor ( key, &gcm_cached_mult[other], mult,
				  sizeof ( *mult ) );
			reduce = gcm_cached_reduce[other];
			reduce ^= be16_to_cpu ( GCM_POLY << 8 );
			gcm_cached_reduce[this] = reduce;

		} else {

			/* Even number: entry[i] = entry[i/2] * (x) */
			other = ( this << 1 ); /* bit-reversed (i / 2) */
			gcm_multiply_x ( &gcm_cached_mult[other], mult );
			reduce = be16_to_cpu ( gcm_cached_reduce[other] );
			reduce >>= 1;
			gcm_cached_reduce[this] = cpu_to_be16 ( reduce );
		}
	}

	/* Record cached key */
	gcm_cached_key = key;
}

/**
 * Multiply polynomial by (x^8) in situ
 *
 * @v poly		Multiplicand and result
 */
static void gcm_multiply_x_8 ( union gcm_block *poly ) {
	uint8_t *byte;
	uint8_t msb;

	/* Reduction table must already have been calculated */
	assert ( gcm_cached_key != NULL );

	/* Record most significant byte */
	byte = &poly->byte[ sizeof ( poly->byte ) - 1 ];
	msb = *byte;

	/* Multiply least significant bytes by shifting */
	for ( ; byte > &poly->byte[0] ; byte-- )
		*byte = *( byte - 1 );
	*byte = 0;

	/* Multiply most significant byte via reduction table */
	poly->word[0] ^= gcm_cached_reduce[msb];
}

/**
 * Multiply polynomial by hash key in situ
 *
 * @v key		Hash key
 * @v poly		Multiplicand and result
 */
static void gcm_multiply_key ( const union gcm_block *key,
			       union gcm_block *poly ) {
	union gcm_block res;
	uint8_t *byte;

	/* Construct tables, if necessary */
	if ( gcm_cached_key != key )
		gcm_cache ( key );

	/* Multiply using Shoup's algorithm */
	byte = &poly->byte[ sizeof ( poly->byte ) - 1 ];
	memcpy ( &res, &gcm_cached_mult[ *byte ], sizeof ( res ) );
	for ( byte-- ; byte >= &poly->byte[0] ; byte-- ) {
		gcm_multiply_x_8 ( &res );
		gcm_xor_block ( &gcm_cached_mult[ *byte ], &res );
	}

	/* Overwrite result */
	memcpy ( poly, &res, sizeof ( *poly ) );
}

/**
 * Encrypt/decrypt/authenticate data
 *
 * @v context		Context
 * @v src		Input data
 * @v dst		Output data, or NULL to process additional data
 * @v len		Length of data
 * @v flags		Operation flags
 */
static void gcm_process ( struct gcm_context *context, const void *src,
			  void *dst, size_t len, unsigned int flags ) {
	union gcm_block tmp;
	uint64_t *total;
	size_t frag_len;
	unsigned int block;

	/* Calculate block number (for debugging) */
	block = ( ( ( context->len.len.add + 8 * sizeof ( tmp ) - 1 ) /
		    ( 8 * sizeof ( tmp ) ) ) +
		  ( ( context->len.len.data + 8 * sizeof ( tmp ) - 1 ) /
		    ( 8 * sizeof ( tmp ) ) ) + 1 );

	/* Update total length (in bits) */
	total = ( ( dst || ( flags & GCM_FL_IV ) ) ?
		  &context->len.len.data : &context->len.len.add );
	*total += ( len * 8 );

	/* Process data */
	for ( ; len ; src += frag_len, len -= frag_len, block++ ) {

		/* Calculate fragment length */
		frag_len = len;
		if ( frag_len > sizeof ( tmp ) )
			frag_len = sizeof ( tmp );

		/* Update hash with input data */
		gcm_xor ( src, &context->hash, &context->hash, frag_len );

		/* Encrypt/decrypt block, if applicable */
		if ( dst ) {

			/* Increment counter */
			gcm_count ( &context->ctr, 1 );

			/* Encrypt counter */
			DBGC2 ( context, "GCM %p Y[%d]:\n", context, block );
			DBGC2_HDA ( context, 0, &context->ctr,
				    sizeof ( context->ctr ) );
			cipher_encrypt ( context->raw_cipher, &context->raw_ctx,
					 &context->ctr, &tmp, sizeof ( tmp ) );
			DBGC2 ( context, "GCM %p E(K,Y[%d]):\n",
				context, block );
			DBGC2_HDA ( context, 0, &tmp, sizeof ( tmp ) );

			/* Encrypt/decrypt data */
			gcm_xor ( src, &tmp, dst, frag_len );
			dst += frag_len;

			/* Update hash with encrypted data, if applicable */
			gcm_xor ( &tmp, &context->hash, &context->hash,
				  ( frag_len & flags ) );
		}

		/* Update hash */
		gcm_multiply_key ( &context->key, &context->hash );
		DBGC2 ( context, "GCM %p X[%d]:\n", context, block );
		DBGC2_HDA ( context, 0, &context->hash,
			    sizeof ( context->hash ) );
	}
}

/**
 * Construct hash
 *
 * @v context		Context
 * @v hash		Hash to fill in
 */
static void gcm_hash ( struct gcm_context *context, union gcm_block *hash ) {

	/* Construct big-endian lengths block */
	hash->len.add = cpu_to_be64 ( context->len.len.add );
	hash->len.data = cpu_to_be64 ( context->len.len.data );
	DBGC2 ( context, "GCM %p len(A)||len(C):\n", context );
	DBGC2_HDA ( context, 0, hash, sizeof ( *hash ) );

	/* Update hash */
	gcm_xor_block ( &context->hash, hash );
	gcm_multiply_key ( &context->key, hash );
	DBGC2 ( context, "GCM %p GHASH(H,A,C):\n", context );
	DBGC2_HDA ( context, 0, hash, sizeof ( *hash ) );
}

/**
 * Construct tag
 *
 * @v context		Context
 * @v tag		Tag
 */
void gcm_tag ( struct gcm_context *context, union gcm_block *tag ) {
	union gcm_block tmp;
	uint32_t offset;

	/* Construct hash */
	gcm_hash ( context, tag );

	/* Construct encrypted initial counter value */
	memcpy ( &tmp, &context->ctr, sizeof ( tmp ) );
	offset = ( ( -context->len.len.data ) / ( 8 * sizeof ( tmp ) ) );
	gcm_count ( &tmp, offset );
	cipher_encrypt ( context->raw_cipher, &context->raw_ctx, &tmp,
			 &tmp, sizeof ( tmp ) );
	DBGC2 ( context, "GCM %p E(K,Y[0]):\n", context );
	DBGC2_HDA ( context, 0, &tmp, sizeof ( tmp ) );

	/* Construct tag */
	gcm_xor_block ( &tmp, tag );
	DBGC2 ( context, "GCM %p T:\n", context );
	DBGC2_HDA ( context, 0, tag, sizeof ( *tag ) );
}

/**
 * Set key
 *
 * @v context		Context
 * @v key		Key
 * @v keylen		Key length
 * @v raw_cipher	Underlying cipher
 * @ret rc		Return status code
 */
int gcm_setkey ( struct gcm_context *context, const void *key, size_t keylen,
		 struct cipher_algorithm *raw_cipher ) {
	int rc;

	/* Initialise GCM context */
	memset ( context, 0, sizeof ( *context ) );
	context->raw_cipher = raw_cipher;

	/* Set underlying block cipher key */
	if ( ( rc = cipher_setkey ( raw_cipher, context->raw_ctx, key,
				    keylen ) ) != 0 )
		return rc;

	/* Construct GCM hash key */
	cipher_encrypt ( raw_cipher, context->raw_ctx, &context->ctr,
			 &context->key, sizeof ( context->key ) );
	DBGC2 ( context, "GCM %p H:\n", context );
	DBGC2_HDA ( context, 0, &context->key, sizeof ( context->key ) );

	/* Reset counter */
	context->ctr.ctr.value = cpu_to_be32 ( 1 );

	/* Construct cached tables */
	gcm_cache ( &context->key );

	return 0;
}

/**
 * Set initialisation vector
 *
 * @v ctx		Context
 * @v iv		Initialisation vector
 * @v ivlen		Initialisation vector length
 */
void gcm_setiv ( struct gcm_context *context, const void *iv, size_t ivlen ) {

	/* Reset non-key state */
	memset ( context, 0, gcm_offset ( key ) );
	build_assert ( gcm_offset ( key ) > gcm_offset ( hash ) );
	build_assert ( gcm_offset ( key ) > gcm_offset ( len ) );
	build_assert ( gcm_offset ( key ) > gcm_offset ( ctr ) );
	build_assert ( gcm_offset ( key ) < gcm_offset ( raw_cipher ) );
	build_assert ( gcm_offset ( key ) < gcm_offset ( raw_ctx ) );

	/* Reset counter */
	context->ctr.ctr.value = cpu_to_be32 ( 1 );

	/* Process initialisation vector */
	if ( ivlen == sizeof ( context->ctr.ctr.iv ) ) {

		/* Initialisation vector is exactly 96 bits, use it as-is */
		memcpy ( context->ctr.ctr.iv, iv, ivlen );

	} else {

		/* Calculate hash over initialisation vector */
		gcm_process ( context, iv, NULL, ivlen, GCM_FL_IV );
		gcm_hash ( context, &context->ctr );
		assert ( context->len.len.add == 0 );

		/* Reset non-key, non-counter state */
		memset ( context, 0, gcm_offset ( ctr ) );
		build_assert ( gcm_offset ( ctr ) > gcm_offset ( hash ) );
		build_assert ( gcm_offset ( ctr ) > gcm_offset ( len ) );
		build_assert ( gcm_offset ( ctr ) < gcm_offset ( key ) );
		build_assert ( gcm_offset ( ctr ) < gcm_offset ( raw_cipher ) );
		build_assert ( gcm_offset ( ctr ) < gcm_offset ( raw_ctx ) );
	}

	DBGC2 ( context, "GCM %p Y[0]:\n", context );
	DBGC2_HDA ( context, 0, &context->ctr, sizeof ( context->ctr ) );
}

/**
 * Encrypt data
 *
 * @v context		Context
 * @v src		Data to encrypt
 * @v dst		Buffer for encrypted data, or NULL for additional data
 * @v len		Length of data
 */
void gcm_encrypt ( struct gcm_context *context, const void *src, void *dst,
		   size_t len ) {

	/* Process data */
	gcm_process ( context, src, dst, len, GCM_FL_ENCRYPT );
}

/**
 * Decrypt data
 *
 * @v context		Context
 * @v src		Data to decrypt
 * @v dst		Buffer for decrypted data, or NULL for additional data
 * @v len		Length of data
 */
void gcm_decrypt ( struct gcm_context *context, const void *src, void *dst,
		   size_t len ) {

	/* Process data */
	gcm_process ( context, src, dst, len, 0 );
}
