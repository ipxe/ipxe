/*
 * Copyright (C) 2009 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ipxe/crypto.h>
#include <ipxe/cbc.h>

/** @file
 *
 * Cipher-block chaining
 *
 */

/**
 * Set key
 *
 * @v cipher		Cipher algorithm
 * @v ctx		Context
 * @v key		Key
 * @v keylen		Key length
 * @ret rc		Return status code
 */
int cbc_setkey ( struct cipher_algorithm *cipher, void *ctx,
		 const void *key, size_t keylen ) {
	struct cipher_algorithm *raw_cipher = cipher->priv;
	size_t blocksize = cipher->blocksize;
	size_t ctxsize = cipher->ctxsize;
	cbc_context_t ( blocksize, ctxsize ) *context = ctx;

	return cipher_setkey ( raw_cipher, context->raw, key, keylen );
}

/**
 * Set initialisation vector
 *
 * @v cipher		Cipher algorithm
 * @v ctx		Context
 * @v iv		Initialisation vector
 * @v ivlen		Initialisation vector length
 * @ret rc		Return status code
 */
int cbc_setiv ( struct cipher_algorithm *cipher, void *ctx,
		const void *iv, size_t ivlen ) {
	size_t blocksize = cipher->blocksize;
	size_t ctxsize = cipher->ctxsize;
	cbc_context_t ( blocksize, ctxsize ) *context = ctx;

	/* Check length */
	if ( ivlen != sizeof ( context->cbc ) )
		return -ENOTSUP;

	/* Record IV */
	memcpy ( context->cbc, iv, sizeof ( context->cbc ) );

	return 0;
}

/**
 * XOR data blocks
 *
 * @v src		Input data
 * @v dst		Second input data and output data buffer
 * @v len		Length of data
 */
static void cbc_xor ( const void *src, void *dst, size_t len ) {
	const uint32_t *srcl = src;
	uint32_t *dstl = dst;
	unsigned int i;

	/* Assume that block sizes will always be dword-aligned, for speed */
	assert ( ( len % sizeof ( *srcl ) ) == 0 );

	for ( i = 0 ; i < ( len / sizeof ( *srcl ) ) ; i++ )
		dstl[i] ^= srcl[i];
}

/**
 * Encrypt data
 *
 * @v cipher		Cipher algorithm
 * @v ctx		Context
 * @v src		Data to encrypt
 * @v dst		Buffer for encrypted data
 * @v len		Length of data
 */
void cbc_encrypt ( struct cipher_algorithm *cipher, void *ctx,
		   const void *src, void *dst, size_t len ) {
	struct cipher_algorithm *raw_cipher = cipher->priv;
	size_t blocksize = cipher->blocksize;
	size_t ctxsize = cipher->ctxsize;
	cbc_context_t ( blocksize, ctxsize ) *context = ctx;

	assert ( ( len % blocksize ) == 0 );

	while ( len ) {
		cbc_xor ( src, context->cbc, blocksize );
		cipher_encrypt ( raw_cipher, context->raw, context->cbc, dst,
				 blocksize );
		memcpy ( context->cbc, dst, blocksize );
		dst += blocksize;
		src += blocksize;
		len -= blocksize;
	}
}

/**
 * Decrypt data
 *
 * @v cipher		Cipher algorithm
 * @v ctx		Context
 * @v src		Data to decrypt
 * @v dst		Buffer for decrypted data
 * @v len		Length of data
 */
void cbc_decrypt ( struct cipher_algorithm *cipher, void *ctx,
		   const void *src, void *dst, size_t len ) {
	struct cipher_algorithm *raw_cipher = cipher->priv;
	size_t blocksize = cipher->blocksize;
	size_t ctxsize = cipher->ctxsize;
	cbc_context_t ( blocksize, ctxsize ) *context = ctx;
	uint8_t next_cbc_ctx[blocksize];

	assert ( ( len % blocksize ) == 0 );

	while ( len ) {
		memcpy ( next_cbc_ctx, src, blocksize );
		cipher_decrypt ( raw_cipher, context->raw, src, dst,
				 blocksize );
		cbc_xor ( context->cbc, dst, blocksize );
		memcpy ( context->cbc, next_cbc_ctx, blocksize );
		dst += blocksize;
		src += blocksize;
		len -= blocksize;
	}
}
