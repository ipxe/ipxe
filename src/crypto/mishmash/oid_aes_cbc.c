/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <ipxe/aes.h>
#include <ipxe/asn1.h>

/** "aes128-cbc" object identifier */
static uint8_t oid_aes_128_cbc[] = { ASN1_OID_AES128_CBC };

/** "aes192-cbc" object identifier */
static uint8_t oid_aes_192_cbc[] = { ASN1_OID_AES192_CBC };

/** "aes256-cbc" object identifier */
static uint8_t oid_aes_256_cbc[] = { ASN1_OID_AES256_CBC };

/** "aes128-cbc" OID-identified algorithm */
struct asn1_algorithm aes_128_cbc_algorithm __asn1_algorithm = {
	.name = "aes128-cbc",
	.cipher = &aes_cbc_algorithm,
	.oid = ASN1_CURSOR ( oid_aes_128_cbc ),
	.parse = asn1_parse_cbc,
};

/** "aes192-cbc" OID-identified algorithm */
struct asn1_algorithm aes_192_cbc_algorithm __asn1_algorithm = {
	.name = "aes192-cbc",
	.cipher = &aes_cbc_algorithm,
	.oid = ASN1_CURSOR ( oid_aes_192_cbc ),
	.parse = asn1_parse_cbc,
};

/** "aes256-cbc" OID-identified algorithm */
struct asn1_algorithm aes_256_cbc_algorithm __asn1_algorithm = {
	.name = "aes256-cbc",
	.cipher = &aes_cbc_algorithm,
	.oid = ASN1_CURSOR ( oid_aes_256_cbc ),
	.parse = asn1_parse_cbc,
};
