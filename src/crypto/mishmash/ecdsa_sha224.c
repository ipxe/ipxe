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
FILE_SECBOOT ( PERMITTED );

#include <ipxe/ecdsa.h>
#include <ipxe/sha256.h>
#include <ipxe/asn1.h>
#include <ipxe/tls.h>

/** "ecdsa-with-SHA224" object identifier */
static uint8_t oid_ecdsa_with_sha224[] = { ASN1_OID_ECDSA_WITH_SHA224 };

/** "ecdsa-with-SHA224" OID-identified algorithm */
struct asn1_algorithm ecdsa_with_sha224_algorithm __asn1_algorithm = {
	.name = "ecdsaWithSHA224",
	.pubkey = &ecdsa_algorithm,
	.digest = &sha224_algorithm,
	.oid = ASN1_CURSOR ( oid_ecdsa_with_sha224 ),
};

/** ECDSA with SHA-224 signature hash algorithm */
struct tls_signature_hash_algorithm
tls_ecdsa_sha224 __tls_sig_hash_algorithm = {
	.code = {
		.signature = TLS_ECDSA_ALGORITHM,
		.hash = TLS_SHA224_ALGORITHM,
	},
	.pubkey = &ecdsa_algorithm,
	.digest = &sha224_algorithm,
};
