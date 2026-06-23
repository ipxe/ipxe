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
 * SHA-512/256 algorithm
 *
 */

#include <ipxe/crypto.h>
#include <ipxe/sha512.h>

/** SHA-512/256 initial digest values */
static const struct sha512_digest sha512_256_init = {
	.h = {
		0x22312194fc2bf72cULL, 0x9f555fa3c84c64c2ULL,
		0x2393b86b6f53b151ULL, 0x963877195940eabdULL,
		0x96283ee2a88effe3ULL, 0xbe5e1e2553863992ULL,
		0x2b0199fc2c85b8aaULL, 0x0eb72ddc81c52ca2ULL,
	}
};

/** SHA-512/256 algorithm */
SHA512_ALGORITHM ( sha512_256, sha512_256_algorithm, sha512_256_init,
		   SHA512_256_DIGEST_SIZE );
