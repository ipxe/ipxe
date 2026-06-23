/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Merkle-Damgård hash algorithms
 *
 */

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <ipxe/crypto.h>
#include <ipxe/mdhash.h>

/**
 * Initialise algorithm
 *
 * @v digest		Digest algorithm
 * @v ctx		MD hash context
 */
void mdhash_init ( struct digest_algorithm *digest, void *ctx ) {
	const struct mdhash_algorithm *mdhash = digest->priv;
	size_t blocksize = digest->blocksize;
	size_t digestsize = mdhash->digestsize;
	mdhash_context_t ( digestsize, blocksize ) *context = ctx;

	/* Zero accumulated data length */
	context->len = 0;

	/* Copy initial digest values */
	memcpy ( &context->dd.digest, mdhash->init, digestsize );
}

/**
 * Accumulate data
 *
 * @v digest		Digest algorithm
 * @v ctx		MD hash context
 * @v data		Data
 * @v len		Length of data
 */
void mdhash_update ( struct digest_algorithm *digest, void *ctx,
		     const void *data, size_t len ) {
	const struct mdhash_algorithm *mdhash = digest->priv;
	size_t blocksize = digest->blocksize;
	size_t digestsize = mdhash->digestsize;
	mdhash_context_t ( digestsize, blocksize ) *context = ctx;
	const uint8_t *byte = data;
	uint8_t copy[digestsize];
	unsigned int toggle;
	unsigned int mask;
	size_t offset;

	/* Accumulate data a byte at a time, swapping endianness as we
	 * go and performing the digest whenever we fill the data
	 * buffer.
	 */
	toggle = mdhash->toggle;
	mask = ( blocksize - 1 );
	while ( len-- ) {
		offset = ( ( context->len++ ^ toggle ) & mask );
		context->dd.data[offset] = *(byte++);
		if ( ( context->len & mask ) == 0 ) {
			DBGC2 ( digest, "MDHASH %s compressing:\n",
				digest->name );
			DBGC2_HDA ( digest, 0, &context->dd.digest,
				    sizeof ( context->dd.digest ) );
			DBGC2_HDA ( digest, 0, &context->dd.data,
				    sizeof ( context->dd.data ) );
			memcpy ( copy, &context->dd.digest, digestsize );
			mdhash->compress ( &context->dd, copy );
			DBGC2 ( digest, "MDHASH %s compressed:\n",
				digest->name );
			DBGC2_HDA ( digest, 0, &context->dd.digest,
				    sizeof ( context->dd.digest ) );
		}
	}
}

/**
 * Generate digest
 *
 * @v digest		Digest algorithm
 * @v ctx		MD hash context
 * @v out		Output buffer
 */
void mdhash_final ( struct digest_algorithm *digest, void *ctx, void *out ) {
	const struct mdhash_algorithm *mdhash = digest->priv;
	size_t blocksize = digest->blocksize;
	size_t digestsize = mdhash->digestsize;
	mdhash_context_t ( digestsize, blocksize ) *context = ctx;
	uint8_t *byte = out;
	union mdhash_len len;
	unsigned int toggle;
	unsigned int mask;
	unsigned int stop;
	unsigned int i;
	uint8_t pad;

	/* Record length before pre-processing */
	len.qword.bits = ( ( ( uint64_t ) context->len ) * 8 );
	len.qword.pad = 0;

	/* Pad with a single "1" bit followed by as many "0" bits as required */
	mask = ( blocksize - 1 );
	stop = ( blocksize - mdhash->len_len );
	pad = 0x80;
	do {
		mdhash_update ( digest, ctx, &pad, sizeof ( pad ) );
		pad = 0x00;
	} while ( ( context->len & mask ) != stop );

	/* Append length (in bits) */
	toggle = mdhash->len_toggle;
	for ( i = 0 ; i < mdhash->len_len ; i++ ) {
		mdhash_update ( digest, ctx, &len.byte[ i ^ toggle ],
				sizeof ( len.byte[ i ^ toggle ] ) );
	}
	assert ( ( context->len & mask ) == 0 );

	/* Copy out final digest */
	toggle = mdhash->toggle;
	for ( i = 0 ; i < digest->digestsize ; i++ )
		byte[i] = context->dd.digest[ i ^ toggle ];
	DBGC ( digest, "MDHASH %s digested:\n", digest->name );
	DBGC_HDA ( digest, 0, out, digest->digestsize );
}
