/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <config/crypto.h>

/** @file
 *
 * Digest command configuration
 *
 */

PROVIDE_REQUIRING_SYMBOL();

/* MD4 */
#if defined ( CRYPTO_DIGEST_MD4 )
REQUIRE_OBJECT ( cmd_md4 );
#endif

/* MD5 is present by default for historical reasons */

/* SHA-1 is present by default for historical reasons */

/* SHA-224 */
#if defined ( CRYPTO_DIGEST_SHA224 )
REQUIRE_OBJECT ( cmd_sha224 );
#endif

/* SHA-256 */
#if defined ( CRYPTO_DIGEST_SHA256 )
REQUIRE_OBJECT ( cmd_sha256 );
#endif

/* SHA-384 */
#if defined ( CRYPTO_DIGEST_SHA384 )
REQUIRE_OBJECT ( cmd_sha384 );
#endif

/* SHA-512 */
#if defined ( CRYPTO_DIGEST_SHA512 )
REQUIRE_OBJECT ( cmd_sha512 );
#endif
