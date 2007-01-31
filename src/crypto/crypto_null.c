/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * @file
 *
 * Null crypto algorithm
 */

#include <string.h>
#include <gpxe/crypto.h>

static void null_init ( void *ctx __unused ) {
	/* Do nothing */
}

static int null_setkey ( void *ctx __unused, void *key __unused,
			 size_t keylen __unused ) {
	/* Do nothing */
	return 0;
}

static void null_encode ( void *ctx __unused, const void *src,
			  void *dst, size_t len ) {
	if ( dst )
		memcpy ( dst, src, len );
}

static void null_decode ( void *ctx __unused, const void *src,
			  void *dst, size_t len ) {
	if ( dst )
		memcpy ( dst, src, len );
}

static void null_final ( void *ctx __unused, void *out __unused ) {
	/* Do nothing */
}

struct crypto_algorithm crypto_null = {
	.name = "null",
	.ctxsize = 0,
	.blocksize = 1,
	.digestsize = 0,
	.init = null_init,
	.setkey = null_setkey,
	.encode = null_encode,
	.decode = null_decode,
	.final = null_final,
};
