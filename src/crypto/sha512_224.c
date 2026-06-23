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
 * SHA-512/224 algorithm
 *
 */

#include <ipxe/crypto.h>
#include <ipxe/sha512.h>

/** SHA-512/224 initial digest values */
static const struct sha512_digest sha512_224_init = {
	.h = {
		0x8c3d37c819544da2ULL, 0x73e1996689dcd4d6ULL,
		0x1dfab7ae32ff9c82ULL, 0x679dd514582f9fcfULL,
		0x0f6d2b697bd44da8ULL, 0x77e36f7304c48942ULL,
		0x3f9d85a86a1d36c8ULL, 0x1112e6ad91d692a1ULL,
	}
};

/** SHA-512/224 algorithm */
SHA512_ALGORITHM ( sha512_224, sha512_224_algorithm, sha512_224_init,
		   SHA512_224_DIGEST_SIZE );
