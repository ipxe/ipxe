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
 * AES Cryptographic Extensions
 *
 */

#include <assert.h>
#include <ipxe/aes.h>

/** A 16-byte vector register */
typedef uint8_t aes_vector_t __attribute__ (( vector_size ( 16 ) ));

/**
 * Encrypt data
 *
 * @v cipher		Cipher algorithm
 * @v ctx		Context
 * @v src		Data to encrypt
 * @v dst		Buffer for encrypted data
 * @v len		Length of data
 */
static __attribute__ (( target ( "+aes" ) )) void
aesce_encrypt ( struct cipher_algorithm *cipher __unused, void *ctx,
		const void *src, void *dst, size_t len ) {
	struct aes_context *aes = aes_context ( ctx );
	const union aes_matrix *key = aes->encrypt.key;
	const union aes_matrix *in = src;
	union aes_matrix *out = dst;
	aes_vector_t tmp[4];

	/* Sanity check */
	assert ( len == sizeof ( *in ) );
	assert ( len == sizeof ( *out ) );

	/* Encrypt
	 *
	 * Each "aese"/"aesmc" pair performs the AddRoundKey,
	 * ShiftRows, SubBytes, and MixColumns operations.  For the
	 * penultimate round we use only "aese" to perform
	 * AddRoundKey, ShiftRows, and SubBytes.  For the final round
	 * we use "eor" to perform a standalone AddRoundKey.
	 *
	 * We use the LD1 instruction to load multiple consecutive
	 * round keys as a single operation, scheduling each load as
	 * early as possible and ensuring that the final round key
	 * always ends up in the same register regardless of the
	 * number of rounds.
	 */
	asm ( /* Rounds 1-4 */
	      "ld1 { %S2.16b - %V2.16b }, [%1], #64\n\t" /* keys 1-4 */
	      "aese %0.16b, %S2.16b\n\t"
	      "aesmc %0.16b, %0.16b\n\t"
	      "aese %0.16b, %T2.16b\n\t"
	      "aesmc %0.16b, %0.16b\n\t"
	      "aese %0.16b, %U2.16b\n\t"
	      "aesmc %0.16b, %0.16b\n\t"
	      "aese %0.16b, %V2.16b\n\t"
	      "ld1 { %S2.16b - %V2.16b }, [%1], #64\n\t" /* keys 5-8 */
	      "aesmc %0.16b, %0.16b\n\t"
	      /* Rounds 5-8 */
	      "aese %0.16b, %S2.16b\n\t"
	      "aesmc %0.16b, %0.16b\n\t"
	      "aese %0.16b, %T2.16b\n\t"
	      "aesmc %0.16b, %0.16b\n\t"
	      "aese %0.16b, %U2.16b\n\t"
	      "ld1 { %S2.16b - %U2.16b }, [%1], #48\n\t" /* keys 9-11 */
	      "aesmc %0.16b, %0.16b\n\t"
	      "aese %0.16b, %V2.16b\n\t"
	      "aesmc %0.16b, %0.16b\n\t"
	      /* Rounds 9-10 */
	      "aese %0.16b, %S2.16b\n\t"
	      "aesmc %0.16b, %0.16b\n\t"
	      "aese %0.16b, %T2.16b\n\t"
	      "tbz %4, #2, 1f\n\t"
	      "aesmc %0.16b, %0.16b\n\t"
	      /* Rounds 11-12 */
	      "aese %0.16b, %U2.16b\n\t"
	      "ld1 { %T2.16b - %U2.16b }, [%1], #32\n\t" /* keys 12-13 */
	      "aesmc %0.16b, %0.16b\n\t"
	      "aese %0.16b, %T2.16b\n\t"
	      "tbz %4, #1, 1f\n\t"
	      "aesmc %0.16b, %0.16b\n\t"
	      /* Rounds 13-14 */
	      "aese %0.16b, %U2.16b\n\t"
	      "ld1 { %T2.16b - %U2.16b }, [%1]\n\t" /* keys 14-15 */
	      "aesmc %0.16b, %0.16b\n\t"
	      "aese %0.16b, %T2.16b\n\t"
	      /* Final round (11, 13, or 15) */
	      "\n1:\n\t"
	      "eor %0.16b, %0.16b, %U2.16b\n\t"
	      : "=w" ( *out ), "+r" ( key ), "=w" ( tmp )
	      : "0" ( *in ), "r" ( aes->rounds ), "m" ( aes->encrypt ) );
}

/**
 * Decrypt data
 *
 * @v cipher		Cipher algorithm
 * @v ctx		Context
 * @v src		Data to encrypt
 * @v dst		Buffer for encrypted data
 * @v len		Length of data
 */
static __attribute__ (( target ( "+aes" ) )) void
aesce_decrypt ( struct cipher_algorithm *cipher __unused, void *ctx,
		const void *src, void *dst, size_t len ) {
	struct aes_context *aes = aes_context ( ctx );
	const union aes_matrix *key = aes->decrypt.key;
	const union aes_matrix *in = src;
	union aes_matrix *out = dst;
	aes_vector_t tmp[4];

	/* Sanity check */
	assert ( len == sizeof ( *in ) );
	assert ( len == sizeof ( *out ) );

	/* Decrypt
	 *
	 * Each "aesd"/"aesimc" pair performs the AddRoundKey,
	 * InvShiftRows, InvSubBytes, and InvMixColumns operations.
	 * For the penultimate round we use only "aesd" to perform
	 * AddRoundKey, InvShiftRows, and InvSubBytes.  For the final
	 * round we use "eor" to perform a standalone AddRoundKey.
	 *
	 * We use the LD1 instruction to load multiple consecutive
	 * round keys as a single operation, scheduling each load as
	 * early as possible and ensuring that the final round key
	 * always ends up in the same register regardless of the
	 * number of rounds.
	 */
	asm ( /* Rounds 1-4 */
	      "ld1 { %S2.16b - %V2.16b }, [%1], #64\n\t" /* keys 1-4 */
	      "aesd %0.16b, %S2.16b\n\t"
	      "aesimc %0.16b, %0.16b\n\t"
	      "aesd %0.16b, %T2.16b\n\t"
	      "aesimc %0.16b, %0.16b\n\t"
	      "aesd %0.16b, %U2.16b\n\t"
	      "aesimc %0.16b, %0.16b\n\t"
	      "aesd %0.16b, %V2.16b\n\t"
	      "ld1 { %S2.16b - %V2.16b }, [%1], #64\n\t" /* keys 5-8 */
	      "aesimc %0.16b, %0.16b\n\t"
	      /* Rounds 5-8 */
	      "aesd %0.16b, %S2.16b\n\t"
	      "aesimc %0.16b, %0.16b\n\t"
	      "aesd %0.16b, %T2.16b\n\t"
	      "aesimc %0.16b, %0.16b\n\t"
	      "aesd %0.16b, %U2.16b\n\t"
	      "ld1 { %S2.16b - %U2.16b }, [%1], #48\n\t" /* keys 9-11 */
	      "aesimc %0.16b, %0.16b\n\t"
	      "aesd %0.16b, %V2.16b\n\t"
	      "aesimc %0.16b, %0.16b\n\t"
	      /* Rounds 9-10 */
	      "aesd %0.16b, %S2.16b\n\t"
	      "aesimc %0.16b, %0.16b\n\t"
	      "aesd %0.16b, %T2.16b\n\t"
	      "tbz %4, #2, 1f\n\t"
	      "aesimc %0.16b, %0.16b\n\t"
	      /* Rounds 11-12 */
	      "aesd %0.16b, %U2.16b\n\t"
	      "ld1 { %T2.16b - %U2.16b }, [%1], #32\n\t" /* keys 12-13 */
	      "aesimc %0.16b, %0.16b\n\t"
	      "aesd %0.16b, %T2.16b\n\t"
	      "tbz %4, #1, 1f\n\t"
	      "aesimc %0.16b, %0.16b\n\t"
	      /* Rounds 13-14 */
	      "aesd %0.16b, %U2.16b\n\t"
	      "ld1 { %T2.16b - %U2.16b }, [%1]\n\t" /* keys 14-15 */
	      "aesimc %0.16b, %0.16b\n\t"
	      "aesd %0.16b, %T2.16b\n\t"
	      /* Final round (11, 13, or 15) */
	      "\n1:\n\t"
	      "eor %0.16b, %0.16b, %U2.16b\n\t"
	      : "=w" ( *out ), "+r" ( key ), "=w" ( tmp )
	      : "0" ( *in ), "r" ( aes->rounds ), "m" ( aes->decrypt ) );
}

/**
 * Enable hardware acceleration (if supported)
 *
 */
void aes_accelerate ( void ) {
	uint64_t aa64isar0;

	/* Read ID_AA64ISAR0_EL1 (will be emulated at EL0) */
	asm ( "mrs %0, ID_AA64ISAR0_EL1" : "=r" ( aa64isar0 ) );

	/* Check for AES instruction support */
	if ( ( aa64isar0 & AA64ISAR0_AES_MASK ) >= AA64ISAR0_AES_AES ) {
		DBGC ( &aes_algorithm, "AES enabled AES-CE acceleration\n" );
		aes_algorithm.encrypt = aesce_encrypt;
		aes_algorithm.decrypt = aesce_decrypt;
	}
}
