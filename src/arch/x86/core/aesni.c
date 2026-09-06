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
 * AES-NI hardware acceleration
 *
 * The AES-NI instructions use SSE registers.  For the sake of not
 * having to think about the possible consequences across all various
 * runtime environments (BIOS/UEFI/Linux), we choose not to enable
 * "-msse" in CFLAGS for this file.  We include ".arch" directives to
 * ensure that the assembler knows that it is permitted to emit the
 * SSE2 and AES-NI instructions, and use a fixed "%xmm0" rather than
 * an "x" constraint (which GCC would consider to be impossible
 * without "-msse").
 *
 * We rely upon the runtime environment to guarantee that using the
 * "%xmm0" register is a safe operation:
 *
 * - For UEFI and Linux, the runtime environment guarantees that SSE
 *   registers are saved and restored across any context switch that
 *   occurs in the middle of the AES calculation.
 *
 * - For BIOS, all C code in iPXE runs with interrupts disabled
 *   (unless it enables interrupts itself, e.g. by calling currticks()
 *   or by executing an explicit "sti" instruction), which guarantees
 *   that no other code will unexpectedly modify the SSE registers in
 *   the middle of the AES operation.
 *
 * We restore the "%xmm0" register after use, to satisfy the
 * constraints of the IA-32 UEFI ABI (which defines all FPU/MMX/SSE
 * registers as callee-saved).  This is the most restrictive of the
 * ABIs for which this file can be built:
 *
 * - IA32 UEFI requires all XMM registers to be preserved
 *
 * - X64 UEFI would allow us to modify any of "%xmm0"-"%xmm5"
 *
 * - BIOS via virt_call() would allow us to modify any of
 *   "%xmm0"-"%xmm7"
 *
 * - System V i386 would allow us to modify any of "%xmm0"-"%xmm7"
 *
 * - System V x86_64 would allow us to modify any of "%xmm0"-"%xmm15"
 *
 */

#include <assert.h>
#include <ipxe/cpuid.h>
#include <ipxe/aes.h>

/**
 * Encrypt data
 *
 * @v cipher		Cipher algorithm
 * @v ctx		Context
 * @v src		Data to encrypt
 * @v dst		Buffer for encrypted data
 * @v len		Length of data
 */
static void aesni_encrypt ( struct cipher_algorithm *cipher __unused,
			    void *ctx, const void *src, void *dst,
			    size_t len ) {
	struct aes_context *aes = aes_context ( ctx );
	const union aes_matrix *key = aes->encrypt.key;
	const union aes_matrix *in = src;
	union aes_matrix *out = dst;
	union aes_matrix save;

	/* Sanity check */
	assert ( len == sizeof ( *in ) );
	assert ( len == sizeof ( *out ) );

	/* Encrypt */
	asm ( /* Allow SSE2 and AES-NI instructions */
	      ".arch .sse2\n\t"
	      ".arch .aes\n\t"
	      /* Preserve XMM register */
	      "movdqu %%xmm0, %1\n\t"
	      /* Initial round (AddRoundKey) */
	      "movdqu %3, %%xmm0\n\t"
	      "pxor (%0), %%xmm0\n\t"
	      /* Intermediate rounds (ShiftRows, SubBytes, MixColumns,
	       * AddRoundKey).
	       */
	      "cmpb $13, %b4\n\t"
	      "jb 2f\n\t"
	      "je 1f\n\t"
	      /* 15 rounds (13 intermediate rounds) */
	      "aesenc 0x10(%0), %%xmm0\n\t"
	      "aesenc 0x20(%0), %%xmm0\n\t"
	      "lea 0x20(%0), %0\n\t"
	      "\n1:\n\t"
	      /* 13+ rounds (11+ intermediate rounds) */
	      "aesenc 0x10(%0), %%xmm0\n\t"
	      "aesenc 0x20(%0), %%xmm0\n\t"
	      "lea 0x20(%0), %0\n\t"
	      "\n2:\n\t"
	      /* 11+ rounds (9+ intermediate rounds) */
	      "aesenc 0x10(%0), %%xmm0\n\t"
	      "aesenc 0x20(%0), %%xmm0\n\t"
	      "aesenc 0x30(%0), %%xmm0\n\t"
	      "aesenc 0x40(%0), %%xmm0\n\t"
	      "aesenc 0x50(%0), %%xmm0\n\t"
	      "aesenc 0x60(%0), %%xmm0\n\t"
	      "aesenc 0x70(%0), %%xmm0\n\t"
	      "aesenc 0x80(%0), %%xmm0\n\t"
	      "aesenc 0x90(%0), %%xmm0\n\t"
	      /* Final round (ShiftRows, SubBytes, AddRoundKey) */
	      "aesenclast 0xa0(%0), %%xmm0\n\t"
	      "movdqu %%xmm0, %2\n\t"
	      /* Restore XMM register */
	      "movdqu %1, %%xmm0\n\t"
	      : "+r" ( key ), "=m" ( save ), "=m" ( *out )
	      : "m" ( *in ), "rm" ( aes->rounds ), "m" ( aes->encrypt ) );
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
static void aesni_decrypt ( struct cipher_algorithm *cipher __unused,
			    void *ctx, const void *src, void *dst,
			    size_t len ) {
	struct aes_context *aes = aes_context ( ctx );
	const union aes_matrix *key = aes->decrypt.key;
	const union aes_matrix *in = src;
	union aes_matrix *out = dst;
	union aes_matrix save;

	/* Sanity check */
	assert ( len == sizeof ( *in ) );
	assert ( len == sizeof ( *out ) );

	/* Decrypt */
	asm ( /* Allow SSE2 and AES-NI instructions */
	      ".arch .sse2\n\t"
	      ".arch .aes\n\t"
	      /* Preserve XMM register */
	      "movdqu %%xmm0, %1\n\t"
	      /* Initial round (AddRoundKey) */
	      "movdqu %3, %%xmm0\n\t"
	      "pxor (%0), %%xmm0\n\t"
	      /* Intermediate rounds (InvShiftRows, InvSubBytes,
	       * InvMixColumns, AddRoundKey).
	       */
	      "cmpb $13, %b4\n\t"
	      "jb 2f\n\t"
	      "je 1f\n\t"
	      /* 15 rounds (13 intermediate rounds) */
	      "aesdec 0x10(%0), %%xmm0\n\t"
	      "aesdec 0x20(%0), %%xmm0\n\t"
	      "lea 0x20(%0), %0\n\t"
	      "\n1:\n\t"
	      /* 13+ rounds (11+ intermediate rounds) */
	      "aesdec 0x10(%0), %%xmm0\n\t"
	      "aesdec 0x20(%0), %%xmm0\n\t"
	      "lea 0x20(%0), %0\n\t"
	      "\n2:\n\t"
	      /* 11+ rounds (9+ intermediate rounds) */
	      "aesdec 0x10(%0), %%xmm0\n\t"
	      "aesdec 0x20(%0), %%xmm0\n\t"
	      "aesdec 0x30(%0), %%xmm0\n\t"
	      "aesdec 0x40(%0), %%xmm0\n\t"
	      "aesdec 0x50(%0), %%xmm0\n\t"
	      "aesdec 0x60(%0), %%xmm0\n\t"
	      "aesdec 0x70(%0), %%xmm0\n\t"
	      "aesdec 0x80(%0), %%xmm0\n\t"
	      "aesdec 0x90(%0), %%xmm0\n\t"
	      /* Final round (InvShiftRows, InvSubBytes, AddRoundKey) */
	      "aesdeclast 0xa0(%0), %%xmm0\n\t"
	      "movdqu %%xmm0, %2\n\t"
	      /* Restore XMM register */
	      "movdqu %1, %%xmm0\n\t"
	      : "+r" ( key ), "=m" ( save ), "=m" ( *out )
	      : "m" ( *in ), "rm" ( aes->rounds ), "m" ( aes->decrypt ) );
}

/**
 * Enable hardware acceleration (if supported)
 *
 */
void aes_accelerate ( void ) {
	struct x86_features features;

	/* Detect AES-NI support */
	x86_features ( &features );
	if ( features.intel.ecx & CPUID_FEATURES_INTEL_ECX_AESNI ) {
		DBGC ( &aes_algorithm, "AES enabled AES-NI acceleration\n" );
		aes_algorithm.encrypt = aesni_encrypt;
		aes_algorithm.decrypt = aesni_decrypt;
	}
}
