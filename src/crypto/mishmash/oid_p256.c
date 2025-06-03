/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
 *
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

#include <byteswap.h>
#include <ipxe/p256.h>
#include <ipxe/asn1.h>
#include <ipxe/tls.h>

/** "prime256v1" object identifier */
static uint8_t oid_prime256v1[] = { ASN1_OID_PRIME256V1 };

/** "prime256v1" OID-identified algorithm */
struct asn1_algorithm prime256v1_algorithm __asn1_algorithm = {
	.name = "prime256v1",
	.curve = &p256_curve,
	.oid = ASN1_CURSOR ( oid_prime256v1 ),
};

/** P-256 named curve */
struct tls_named_curve tls_secp256r1_named_curve __tls_named_curve ( 01 ) = {
	.curve = &p256_curve,
	.code = htons ( TLS_NAMED_CURVE_SECP256R1 ),
	.format = TLS_POINT_FORMAT_UNCOMPRESSED,
	.pre_master_secret_len = P256_LEN,
};
