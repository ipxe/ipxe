/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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

FILE_LICENCE ( GPL2_OR_LATER );

/** @file
 *
 * Self-test collection
 *
 */

/* Drag in all applicable self-tests */
REQUIRE_OBJECT ( list_test );
REQUIRE_OBJECT ( byteswap_test );
REQUIRE_OBJECT ( settings_test );
REQUIRE_OBJECT ( time_test );
REQUIRE_OBJECT ( crc32_test );
REQUIRE_OBJECT ( md5_test );
REQUIRE_OBJECT ( sha1_test );
REQUIRE_OBJECT ( sha256_test );
REQUIRE_OBJECT ( aes_cbc_test );
REQUIRE_OBJECT ( hmac_drbg_test );
REQUIRE_OBJECT ( hash_df_test );
REQUIRE_OBJECT ( bigint_test );
REQUIRE_OBJECT ( rsa_test );
REQUIRE_OBJECT ( x509_test );
REQUIRE_OBJECT ( ocsp_test );
REQUIRE_OBJECT ( cms_test );
