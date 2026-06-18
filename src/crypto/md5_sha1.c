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
 * Hybrid MD5+SHA1 hash as used by TLSv1.1 and earlier
 *
 */

#include <ipxe/crypto.h>
#include <ipxe/md5_sha1.h>

/**
 * Initialise MD5+SHA1 algorithm
 *
 * @v digest		Digest algorithm
 * @v ctx		MD5+SHA1 context
 */
static void md5_sha1_init ( struct digest_algorithm *digest __unused,
			    void *ctx ) {
	struct md5_sha1_context *context = ctx;

	digest_init ( &md5_algorithm, context->md5 );
	digest_init ( &sha1_algorithm, context->sha1 );
}

/**
 * Accumulate data with MD5+SHA1 algorithm
 *
 * @v digest		Digest algorithm
 * @v ctx		MD5+SHA1 context
 * @v data		Data
 * @v len		Length of data
 */
static void md5_sha1_update ( struct digest_algorithm *digest __unused,
			      void *ctx, const void *data, size_t len ) {
	struct md5_sha1_context *context = ctx;

	digest_update ( &md5_algorithm, context->md5, data, len );
	digest_update ( &sha1_algorithm, context->sha1, data, len );
}

/**
 * Generate MD5+SHA1 digest
 *
 * @v digest		Digest algorithm
 * @v ctx		MD5+SHA1 context
 * @v out		Output buffer
 */
static void md5_sha1_final ( struct digest_algorithm *digest __unused,
			     void *ctx, void *out ) {
	struct md5_sha1_context *context = ctx;
	struct md5_sha1_digest *output = out;

	digest_final ( &md5_algorithm, context->md5, output->md5 );
	digest_final ( &sha1_algorithm, context->sha1, output->sha1 );
}

/** Hybrid MD5+SHA1 digest algorithm */
struct digest_algorithm md5_sha1_algorithm = {
	.name		= "md5+sha1",
	.ctxsize	= sizeof ( struct md5_sha1_context ),
	.blocksize	= 0, /* Not applicable */
	.digestsize	= sizeof ( struct md5_sha1_digest ),
	.init		= md5_sha1_init,
	.update		= md5_sha1_update,
	.final		= md5_sha1_final,
};
