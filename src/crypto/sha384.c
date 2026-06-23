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
 * SHA-384 algorithm
 *
 */

#include <ipxe/crypto.h>
#include <ipxe/sha512.h>

/** SHA-384 initial digest values */
static const struct sha512_digest sha384_init = {
	.h = {
		0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL,
		0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
		0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL,
		0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL,
	}
};

/** SHA-384 algorithm */
SHA512_ALGORITHM ( sha384, sha384_algorithm, sha384_init, SHA384_DIGEST_SIZE );
