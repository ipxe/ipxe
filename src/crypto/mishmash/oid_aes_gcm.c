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

/** "aes128-gcm" object identifier */
static uint8_t oid_aes_128_gcm[] = { ASN1_OID_AES128_GCM };

/** "aes192-gcm" object identifier */
static uint8_t oid_aes_192_gcm[] = { ASN1_OID_AES192_GCM };

/** "aes256-gcm" object identifier */
static uint8_t oid_aes_256_gcm[] = { ASN1_OID_AES256_GCM };

/** "aes128-gcm" OID-identified algorithm */
struct asn1_algorithm aes_128_gcm_algorithm __asn1_algorithm = {
	.name = "aes128-gcm",
	.cipher = &aes_gcm_algorithm,
	.oid = ASN1_CURSOR ( oid_aes_128_gcm ),
	.parse = asn1_parse_gcm,
};

/** "aes192-gcm" OID-identified algorithm */
struct asn1_algorithm aes_192_gcm_algorithm __asn1_algorithm = {
	.name = "aes192-gcm",
	.cipher = &aes_gcm_algorithm,
	.oid = ASN1_CURSOR ( oid_aes_192_gcm ),
	.parse = asn1_parse_gcm,
};

/** "aes256-gcm" OID-identified algorithm */
struct asn1_algorithm aes_256_gcm_algorithm __asn1_algorithm = {
	.name = "aes256-gcm",
	.cipher = &aes_gcm_algorithm,
	.oid = ASN1_CURSOR ( oid_aes_256_gcm ),
	.parse = asn1_parse_gcm,
};
