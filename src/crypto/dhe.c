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
 * Ephemeral Diffie-Hellman key exchange
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <ipxe/bigint.h>
#include <ipxe/dhe.h>

/**
 * Calculate Diffie-Hellman key
 *
 * @v modulus		Prime modulus
 * @v len		Length of prime modulus
 * @v generator		Generator
 * @v generator_len	Length of generator
 * @v partner		Partner public key
 * @v partner_len	Length of partner public key
 * @v private		Private key
 * @v private_len	Length of private key
 * @ret public		Public key (length equal to prime modulus)
 * @ret shared		Shared secret (length equal to prime modulus)
 * @ret rc		Return status code
 */
int dhe_key ( const void *modulus, size_t len, const void *generator,
	      size_t generator_len, const void *partner, size_t partner_len,
	      const void *private, size_t private_len, void *public,
	      void *shared ) {
	unsigned int size = bigint_required_size ( len );
	unsigned int private_size = bigint_required_size ( private_len );
	bigint_t ( size ) *mod;
	size_t tmp_len = bigint_mod_exp_tmp_len ( mod );
	struct {
		bigint_t ( size ) modulus;
		bigint_t ( size ) generator;
		bigint_t ( size ) partner;
		bigint_t ( private_size ) private;
		bigint_t ( size ) result;
		uint8_t tmp[tmp_len];
	} __attribute__ (( packed )) *ctx;
	int rc;

	DBGC2 ( modulus, "DHE %p modulus:\n", modulus );
	DBGC2_HDA ( modulus, 0, modulus, len );
	DBGC2 ( modulus, "DHE %p generator:\n", modulus );
	DBGC2_HDA ( modulus, 0, generator, generator_len );
	DBGC2 ( modulus, "DHE %p partner public key:\n", modulus );
	DBGC2_HDA ( modulus, 0, partner, partner_len );
	DBGC2 ( modulus, "DHE %p private key:\n", modulus );
	DBGC2_HDA ( modulus, 0, private, private_len );

	/* Sanity checks */
	if ( generator_len > len ) {
		DBGC ( modulus, "DHE %p overlength generator\n", modulus );
		rc = -EINVAL;
		goto err_sanity;
	}
	if ( partner_len > len ) {
		DBGC ( modulus, "DHE %p overlength partner public key\n",
		       modulus );
		rc = -EINVAL;
		goto err_sanity;
	}
	if ( private_len > len ) {
		DBGC ( modulus, "DHE %p overlength private key\n", modulus );
		rc = -EINVAL;
		goto err_sanity;
	}

	/* Allocate context */
	ctx = malloc ( sizeof ( *ctx ) );
	if ( ! ctx ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Initialise context */
	bigint_init ( &ctx->modulus, modulus, len );
	bigint_init ( &ctx->generator, generator, generator_len );
	bigint_init ( &ctx->partner, partner, partner_len );
	bigint_init ( &ctx->private, private, private_len );

	/* Calculate public key */
	bigint_mod_exp ( &ctx->generator, &ctx->modulus, &ctx->private,
			 &ctx->result, ctx->tmp );
	bigint_done ( &ctx->result, public, len );
	DBGC2 ( modulus, "DHE %p public key:\n", modulus );
	DBGC2_HDA ( modulus, 0, public, len );

	/* Calculate shared secret */
	bigint_mod_exp ( &ctx->partner, &ctx->modulus, &ctx->private,
			 &ctx->result, ctx->tmp );
	bigint_done ( &ctx->result, shared, len );
	DBGC2 ( modulus, "DHE %p shared secret:\n", modulus );
	DBGC2_HDA ( modulus, 0, shared, len );

	/* Success */
	rc = 0;

	free ( ctx );
 err_alloc:
 err_sanity:
	return rc;
}
