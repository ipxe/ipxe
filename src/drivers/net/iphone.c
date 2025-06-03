/*
 * Copyright (C) 2020 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/profile.h>
#include <ipxe/base64.h>
#include <ipxe/sha256.h>
#include <ipxe/rsa.h>
#include <ipxe/x509.h>
#include <ipxe/pem.h>
#include <ipxe/xfer.h>
#include <ipxe/tls.h>
#include <ipxe/usb.h>
#include "iphone.h"

/** @file
 *
 * iPhone USB Ethernet driver
 *
 */

/* Disambiguate the various error causes */
#define EPIPE_NO_MUX __einfo_error ( EINFO_EPIPE_NO_MUX )
#define EINFO_EPIPE_NO_MUX						\
	__einfo_uniqify ( EINFO_EPIPE, 0x01,				\
			  "No USB multiplexer" )
#define EINPROGRESS_PAIRING __einfo_error ( EINFO_EINPROGRESS_PAIRING )
#define EINFO_EINPROGRESS_PAIRING					\
	__einfo_uniqify ( EINFO_EINPROGRESS, 0x01,			\
			  "Pairing in progress" )
#define ENOTCONN_DISABLED __einfo_error ( EINFO_ENOTCONN_DISABLED )
#define EINFO_ENOTCONN_DISABLED						\
	__einfo_uniqify ( EINFO_ENOTCONN, IPHONE_LINK_DISABLED,		\
			  "Personal Hotspot disabled" )
#define ENOTCONN_STATUS( status )					\
	EUNIQ ( EINFO_ENOTCONN, ( (status) & 0x1f ),			\
		ENOTCONN_DISABLED )

static int ipair_create ( struct interface *xfer, unsigned int flags );

/** Bulk IN completion profiler */
static struct profiler iphone_in_profiler __profiler =
	{ .name = "iphone.in" };

/** Bulk OUT profiler */
static struct profiler iphone_out_profiler __profiler =
	{ .name = "iphone.out" };

/** List of USB multiplexers */
static LIST_HEAD ( imuxes );

/** List of iPhone network devices */
static LIST_HEAD ( iphones );

/******************************************************************************
 *
 * iPhone pairing certificates
 *
 ******************************************************************************
 */

/** iPhone root certificate fingerprint */
static uint8_t icert_root_fingerprint[SHA256_DIGEST_SIZE];

/** Root of trust for iPhone certificates */
static struct x509_root icert_root = {
	.refcnt = REF_INIT ( ref_no_free ),
	.digest = &sha256_algorithm,
	.count = 1,
	.fingerprints = icert_root_fingerprint,
};

/** Single zero byte used in constructed certificates */
static const uint8_t icert_nul[] = { 0x00 };

/** "RSA algorithm" identifier used in constructed certificates */
static const uint8_t icert_rsa[] = {
	/* algorithm */
	ASN1_SHORT ( ASN1_SEQUENCE,
		     ASN1_SHORT ( ASN1_OID, ASN1_OID_RSAENCRYPTION ),
		     ASN1_NULL, 0x00 )
};

/** "SHA-256 with RSA algorithm" identifier used in constructed certificates */
static const uint8_t icert_sha256_rsa[] = {
	ASN1_SHORT ( ASN1_SEQUENCE,
		     ASN1_SHORT ( ASN1_OID, ASN1_OID_SHA256WITHRSAENCRYPTION ),
		     ASN1_NULL, 0x00 ),
};

/** Extensions used in constructed root certificate */
static const uint8_t icert_root_exts_data[] = {
	/* extensions */
	ASN1_SHORT ( ASN1_EXPLICIT_TAG ( 3 ), ASN1_SHORT ( ASN1_SEQUENCE,
	/* basicConstraints */
	ASN1_SHORT ( ASN1_SEQUENCE,
		     /* extnID */
		     ASN1_SHORT ( ASN1_OID, ASN1_OID_BASICCONSTRAINTS ),
		     /* critical */
		     ASN1_SHORT ( ASN1_BOOLEAN, 0xff ),
		     /* extnValue */
		     ASN1_SHORT ( ASN1_OCTET_STRING,
				  ASN1_SHORT ( ASN1_SEQUENCE,
					       ASN1_SHORT ( ASN1_BOOLEAN,
							    0xff ) ) ) ) ) )
};

/** Extensions used in constructed root certificate */
static struct asn1_cursor icert_root_exts =
	ASN1_CURSOR ( icert_root_exts_data );

/** Extensions used in constructed leaf certificates */
static const uint8_t icert_leaf_exts_data[] = {
	/* extensions */
	ASN1_SHORT ( ASN1_EXPLICIT_TAG ( 3 ), ASN1_SHORT ( ASN1_SEQUENCE,
	/* basicConstraints */
	ASN1_SHORT ( ASN1_SEQUENCE,
		     /* extnID */
		     ASN1_SHORT ( ASN1_OID, ASN1_OID_BASICCONSTRAINTS ),
		     /* critical */
		     ASN1_SHORT ( ASN1_BOOLEAN, 0xff ),
		     /* extnValue */
		     ASN1_SHORT ( ASN1_OCTET_STRING,
				  ASN1_SHORT ( ASN1_SEQUENCE,
					       ASN1_SHORT ( ASN1_BOOLEAN,
							    0x00 ) ) ) ),
	/* keyUsage */
	ASN1_SHORT ( ASN1_SEQUENCE,
		     /* extnID */
		     ASN1_SHORT ( ASN1_OID, ASN1_OID_KEYUSAGE ),
		     /* critical */
		     ASN1_SHORT ( ASN1_BOOLEAN, 0xff ),
		     /* extnValue */
		     ASN1_SHORT ( ASN1_OCTET_STRING,
				  ASN1_SHORT ( ASN1_BIT_STRING, 0x07,
					       ( X509_DIGITAL_SIGNATURE |
						 X509_KEY_ENCIPHERMENT ),
					       0x00 ) ) ) ) )
};

/** Extensions used in constructed leaf certificates */
static struct asn1_cursor icert_leaf_exts =
	ASN1_CURSOR ( icert_leaf_exts_data );

/** "TBSCertificate" prefix in constructed certificates */
static const uint8_t icert_tbs_prefix[] = {
	/* version */
	ASN1_SHORT ( ASN1_EXPLICIT_TAG ( 0 ), ASN1_SHORT ( ASN1_INTEGER, 2 ) ),
	/* serialNumber */
	ASN1_SHORT ( ASN1_INTEGER, 0 ),
	/* signature */
	ASN1_SHORT ( ASN1_SEQUENCE,
		     ASN1_SHORT ( ASN1_OID, ASN1_OID_SHA256WITHRSAENCRYPTION ),
		     ASN1_NULL, 0x00 )
};

/** Validity period in constructed certificates */
static const uint8_t icert_validity[] = {
	/* validity */
	ASN1_SHORT ( ASN1_SEQUENCE,
		     /* notBefore */
		     ASN1_SHORT ( ASN1_GENERALIZED_TIME,
				  '1', '9', '7', '8', '1', '2', '1', '0',
				  '2', '2', '0', '0', '0', '0', 'Z' ),
		     /* notAfter */
		     ASN1_SHORT ( ASN1_GENERALIZED_TIME,
				  '2', '9', '9', '9', '0', '1', '0', '1',
				  '0', '0', '0', '0', '0', '0', 'Z' ) )
};

/** "Root" subject name */
static const uint8_t icert_name_root_data[] = {
	ASN1_SHORT ( ASN1_SEQUENCE, ASN1_SHORT ( ASN1_SET,
	ASN1_SHORT ( ASN1_SEQUENCE,
		     ASN1_SHORT ( ASN1_OID, ASN1_OID_COMMON_NAME ),
		     ASN1_SHORT ( ASN1_UTF8_STRING, 'R', 'o', 'o', 't' ) ) ) )
};

/** "Root" subject name */
static struct asn1_cursor icert_name_root =
	ASN1_CURSOR ( icert_name_root_data );

/** "iPXE" subject name */
static const uint8_t icert_name_ipxe_data[] = {
	ASN1_SHORT ( ASN1_SEQUENCE, ASN1_SHORT ( ASN1_SET,
	ASN1_SHORT ( ASN1_SEQUENCE,
		     ASN1_SHORT ( ASN1_OID, ASN1_OID_COMMON_NAME ),
		     ASN1_SHORT ( ASN1_UTF8_STRING, 'i', 'P', 'X', 'E' ) ) ) )
};

/** "iPXE" subject name */
static struct asn1_cursor icert_name_ipxe =
	ASN1_CURSOR ( icert_name_ipxe_data );

/** "iPhone" subject name */
static const uint8_t icert_name_iphone_data[] = {
	ASN1_SHORT ( ASN1_SEQUENCE, ASN1_SHORT ( ASN1_SET,
	ASN1_SHORT ( ASN1_SEQUENCE,
		     ASN1_SHORT ( ASN1_OID, ASN1_OID_COMMON_NAME ),
		     ASN1_SHORT ( ASN1_UTF8_STRING,
				  'i', 'P', 'h', 'o', 'n', 'e' ) ) ) )
};

/** "iPhone" subject name */
static struct asn1_cursor icert_name_iphone =
	ASN1_CURSOR ( icert_name_iphone_data );

/** Public key(s) used for pairing */
static const uint8_t icert_public_a[] __unused = {
	0x02, 0x81, 0x81, 0x00, 0xc9, 0xc0, 0xdd, 0xa6, 0xd5, 0xf9, 0x05, 0x3e,
	0x1d, 0xcb, 0x67, 0x08, 0xa8, 0x50, 0x27, 0x63, 0x95, 0x87, 0x42, 0x7e,
	0xfb, 0xff, 0x55, 0x55, 0xb8, 0xc0, 0x6f, 0x13, 0xcb, 0xf7, 0xc5, 0x1b,
	0xda, 0x44, 0x3c, 0xbc, 0x1a, 0xe1, 0x15, 0x1e, 0xab, 0x56, 0x74, 0x02,
	0x8b, 0xb3, 0xcd, 0x42, 0x56, 0xcd, 0x9c, 0xc3, 0x15, 0xe2, 0x33, 0x97,
	0x6d, 0x77, 0xdd, 0x20, 0x3a, 0x74, 0xb1, 0x4c, 0xee, 0xeb, 0xe8, 0xaa,
	0x20, 0x71, 0x5a, 0xa2, 0x5b, 0xf8, 0x1a, 0xcb, 0xd2, 0x7b, 0x96, 0xb6,
	0x42, 0xb4, 0x7c, 0x7a, 0x13, 0xec, 0x55, 0xd3, 0x36, 0x8b, 0xe3, 0x17,
	0xc5, 0xc4, 0xcc, 0xe0, 0x27, 0x8c, 0xed, 0xa1, 0x4c, 0x8a, 0x50, 0x4a,
	0x1c, 0xc4, 0x58, 0xf6, 0xcd, 0xcc, 0xc3, 0x5f, 0xe6, 0x3c, 0xff, 0x97,
	0x51, 0xed, 0xf5, 0xaa, 0x89, 0xcc, 0x3f, 0x63, 0x67, 0x46, 0x9f, 0xbf,
	0x02, 0x03, 0x01, 0x00, 0x01
};
static const uint8_t icert_public_b[] __unused = {
	0x02, 0x81, 0x81, 0x00, 0xcd, 0x96, 0x81, 0x78, 0xbb, 0x2e, 0x64, 0xda,
	0xd3, 0x7e, 0xd7, 0x3a, 0xac, 0x3f, 0x00, 0xe5, 0x41, 0x65, 0x56, 0xac,
	0x2d, 0x77, 0xc0, 0x1a, 0xad, 0x32, 0xca, 0x0c, 0x72, 0xae, 0xdb, 0x57,
	0xc1, 0xc7, 0x79, 0xef, 0xc6, 0x71, 0x9f, 0xad, 0x82, 0x14, 0x94, 0x4b,
	0xf9, 0xd8, 0x78, 0xf1, 0xca, 0x99, 0xf5, 0x71, 0x07, 0x88, 0xd7, 0x55,
	0xc7, 0xcb, 0x36, 0x5d, 0xdb, 0x84, 0x46, 0xac, 0x05, 0xea, 0xf1, 0xe1,
	0xbe, 0x91, 0x50, 0x85, 0x1e, 0x64, 0xab, 0x02, 0x82, 0xab, 0xba, 0x42,
	0x06, 0x5a, 0xe3, 0xc3, 0x25, 0xd0, 0x95, 0x04, 0x54, 0xb4, 0x44, 0x40,
	0x5a, 0x42, 0x06, 0x04, 0x7d, 0x3b, 0x9e, 0xaf, 0x2e, 0xe9, 0xc8, 0xad,
	0x46, 0x3a, 0xff, 0xe2, 0x39, 0xc8, 0x48, 0x0a, 0x49, 0xaa, 0xfe, 0x1f,
	0x6c, 0x91, 0x5d, 0x1d, 0xd6, 0xb0, 0x04, 0xd1, 0x6c, 0xb2, 0x43, 0xaf,
	0x02, 0x03, 0x01, 0x00, 0x01
};

/**
 * "Private" key(s) used for pairing
 *
 * Yes, this publicly visible "private" key completely obviates any
 * nominal security provided by the pairing process.  Looked at
 * another way, this modifies the iPhone to behave like every other
 * USB tethering device: if the cable is physically connected and
 * tethering is enabled then the device will Just Work.
 *
 * Unlike Android, the iPhone seems to have no meaningful permissions
 * model: any device that is trusted to use the phone for tethering
 * seems to also be trusted to use the iPhone for any other purpose
 * (e.g. accessing files, reading messages, etc).  Apple should
 * probably fix this at some point, e.g. via defining extended key
 * usages in the root and host certificates.
 */
static const uint8_t icert_private_a[] __unused = {
	0x02, 0x81, 0x80, 0x1d, 0x60, 0xb7, 0x25, 0xdf, 0x0c, 0x76, 0xc5, 0xf7,
	0xc2, 0xb1, 0x8b, 0x22, 0x2f, 0x21, 0xbd, 0x2f, 0x7d, 0xd5, 0xa1, 0xf6,
	0x01, 0xd5, 0x24, 0x39, 0x55, 0xd4, 0x16, 0xd6, 0xe1, 0x8a, 0x53, 0x26,
	0xf2, 0x3e, 0xc1, 0xc9, 0x4c, 0x33, 0x2e, 0x17, 0x16, 0xec, 0xa7, 0x9e,
	0x3e, 0x1d, 0x4a, 0x66, 0xa7, 0x64, 0x07, 0x48, 0x3d, 0x7a, 0xf3, 0xb6,
	0xdd, 0xf8, 0x56, 0x04, 0x0d, 0x0f, 0xef, 0xf8, 0xbd, 0xbc, 0x73, 0xe2,
	0xc2, 0xae, 0x1b, 0x87, 0x90, 0x18, 0x2a, 0x68, 0xff, 0xae, 0x49, 0xdf,
	0x7c, 0xff, 0xe8, 0x44, 0xa8, 0x3e, 0x4e, 0x4f, 0xf5, 0xfa, 0x51, 0x96,
	0xb8, 0x08, 0xf3, 0x18, 0xd6, 0x52, 0xdf, 0x3a, 0x8a, 0xed, 0xda, 0xcd,
	0xb4, 0x06, 0x99, 0x41, 0xcb, 0x23, 0x17, 0xaf, 0xc3, 0x3e, 0xfe, 0xdf,
	0x97, 0xf3, 0xd6, 0x18, 0x7e, 0x03, 0xaf, 0x62, 0xb2, 0xc8, 0xc9
};
static const uint8_t icert_private_b[] __unused = {
	0x02, 0x81, 0x80, 0x45, 0xbd, 0xc0, 0xbe, 0x0c, 0x01, 0x79, 0x05, 0x22,
	0xa9, 0xec, 0xa9, 0x62, 0xb5, 0x1c, 0xc0, 0xa8, 0xa6, 0x8f, 0xf8, 0x68,
	0x94, 0x2e, 0xfe, 0xdd, 0xb2, 0x55, 0x08, 0x53, 0xff, 0x2d, 0x39, 0x5f,
	0xeb, 0x23, 0x5a, 0x4b, 0x9f, 0x4f, 0xe3, 0xb4, 0x34, 0xf6, 0xf9, 0xaf,
	0x0f, 0xd8, 0x37, 0x6d, 0xdb, 0x3c, 0x7f, 0xd3, 0x66, 0x80, 0x66, 0x01,
	0x18, 0xd6, 0xa0, 0x90, 0x4f, 0x17, 0x09, 0xb8, 0x68, 0x44, 0xf0, 0xde,
	0x16, 0x4a, 0x8a, 0x0d, 0xa7, 0x5f, 0xb5, 0x4c, 0x53, 0xcc, 0x21, 0xdd,
	0x4f, 0x05, 0x64, 0xa5, 0xc5, 0xac, 0x2c, 0xd8, 0x0a, 0x7b, 0xf5, 0xa4,
	0x63, 0x32, 0xb0, 0x2c, 0xf8, 0xef, 0x8c, 0xf8, 0x2c, 0xba, 0x1c, 0x2c,
	0xc7, 0x0a, 0xf3, 0xe9, 0x8f, 0xfb, 0x0a, 0x61, 0x1b, 0x3a, 0xdd, 0x9f,
	0x74, 0x7d, 0xb3, 0x42, 0x59, 0x52, 0x07, 0x59, 0x8e, 0xb7, 0x41
};

/** Key pair selection
 *
 * This exists only to allow for testing of the process for handling a
 * failed TLS negotiation.
 */
#define icert_key_suffix a
#define icert_key_variable( prefix ) _C2 ( prefix, icert_key_suffix )
#define icert_public icert_key_variable ( icert_public_ )
#define icert_private icert_key_variable ( icert_private_ )

/** PEM certificate prefix */
static const char icert_begin[] = "-----BEGIN CERTIFICATE-----\n";

/** PEM certificate suffix */
static const char icert_end[] = "\n-----END CERTIFICATE-----\n";

/**
 * Free pairing certificates
 *
 * @v icert		Pairing certificates
 */
static void icert_free ( struct icert *icert ) {

	privkey_put ( icert->key );
	x509_put ( icert->root );
	x509_put ( icert->host );
	x509_put ( icert->device );
	memset ( icert, 0, sizeof ( *icert ) );
}

/**
 * Construct certificate
 *
 * @v icert		Pairing certificates
 * @v subject		Subject name
 * @v issuer		Issuer name
 * @v private		Private key
 * @v public		Public key
 * @v exts		Certificate extensions
 * @v cert		Certificate to fill in
 * @ret rc		Return status code
 *
 * On success, the caller is responsible for eventually calling
 * x509_put() on the allocated encoded certificate.
 */
static int icert_cert ( struct icert *icert, struct asn1_cursor *subject,
			struct asn1_cursor *issuer, struct asn1_cursor *private,
			struct asn1_cursor *public, struct asn1_cursor *exts,
			struct x509_certificate **cert ) {
	struct digest_algorithm *digest = &sha256_algorithm;
	struct pubkey_algorithm *pubkey = &rsa_algorithm;
	struct asn1_builder spki = { NULL, 0 };
	struct asn1_builder tbs = { NULL, 0 };
	struct asn1_builder raw = { NULL, 0 };
	uint8_t digest_ctx[SHA256_CTX_SIZE];
	uint8_t digest_out[SHA256_DIGEST_SIZE];
	int len;
	int rc;

	/* Construct subjectPublicKeyInfo */
	if ( ( rc = ( asn1_prepend_raw ( &spki, public->data, public->len ),
		      asn1_prepend_raw ( &spki, icert_nul,
					 sizeof ( icert_nul ) ),
		      asn1_wrap ( &spki, ASN1_BIT_STRING ),
		      asn1_prepend_raw ( &spki, icert_rsa,
					 sizeof ( icert_rsa ) ),
		      asn1_wrap ( &spki, ASN1_SEQUENCE ) ) ) != 0 ) {
		DBGC ( icert, "ICERT %p could not build subjectPublicKeyInfo: "
		       "%s\n", icert, strerror ( rc ) );
		goto err_spki;
	}

	/* Construct tbsCertificate */
	if ( ( rc = ( asn1_prepend_raw ( &tbs, exts->data, exts->len ),
		      asn1_prepend_raw ( &tbs, spki.data, spki.len ),
		      asn1_prepend_raw ( &tbs, subject->data, subject->len ),
		      asn1_prepend_raw ( &tbs, icert_validity,
					 sizeof ( icert_validity ) ),
		      asn1_prepend_raw ( &tbs, issuer->data, issuer->len ),
		      asn1_prepend_raw ( &tbs, icert_tbs_prefix,
					 sizeof ( icert_tbs_prefix ) ),
		      asn1_wrap ( &tbs, ASN1_SEQUENCE ) ) ) != 0 ) {
		DBGC ( icert, "ICERT %p could not build tbsCertificate: %s\n",
		       icert, strerror ( rc ) );
		goto err_tbs;
	}

	/* Calculate certificate digest */
	digest_init ( digest, digest_ctx );
	digest_update ( digest, digest_ctx, tbs.data, tbs.len );
	digest_final ( digest, digest_ctx, digest_out );

	/* Construct signature using "private" key */
	if ( ( rc = asn1_grow ( &raw,
				pubkey_max_len ( pubkey, private ) ) ) != 0 ) {
		DBGC ( icert, "ICERT %p could not build signature: %s\n",
		       icert, strerror ( rc ) );
		goto err_grow;
	}
	if ( ( len = pubkey_sign ( pubkey, private, digest, digest_out,
				   raw.data ) ) < 0 ) {
		rc = len;
		DBGC ( icert, "ICERT %p could not sign: %s\n",
		       icert, strerror ( rc ) );
		goto err_pubkey_sign;
	}
	assert ( ( ( size_t ) len ) == raw.len );

	/* Construct raw certificate data */
	if ( ( rc = ( asn1_prepend_raw ( &raw, icert_nul,
					 sizeof ( icert_nul ) ),
		      asn1_wrap ( &raw, ASN1_BIT_STRING ),
		      asn1_prepend_raw ( &raw, icert_sha256_rsa,
					 sizeof ( icert_sha256_rsa ) ),
		      asn1_prepend_raw ( &raw, tbs.data, tbs.len ),
		      asn1_wrap ( &raw, ASN1_SEQUENCE ) ) ) != 0 ) {
		DBGC ( icert, "ICERT %p could not build certificate: %s\n",
		       icert, strerror ( rc ) );
		goto err_raw;
	}

	/* Parse certificate */
	if ( ( rc = x509_certificate ( raw.data, raw.len, cert ) ) != 0 ) {
		DBGC ( icert, "ICERT %p invalid certificate: %s\n",
		       icert, strerror ( rc ) );
		DBGC_HDA ( icert, 0, raw.data, raw.len );
		goto err_x509;
	}

 err_x509:
 err_raw:
 err_pubkey_sign:
	free ( raw.data );
 err_grow:
	free ( tbs.data );
 err_tbs:
	free ( spki.data );
 err_spki:
	return rc;
}

/**
 * Construct certificates
 *
 * @v icert		Certificate set
 * @v pubkey		Device public key
 * @ret rc		Return status code
 */
static int icert_certs ( struct icert *icert, struct asn1_cursor *key ) {
	struct digest_algorithm *digest = icert_root.digest;
	struct asn1_builder public = { NULL, 0 };
	struct asn1_builder *private;
	int rc;

	/* Free any existing key and certificates */
	icert_free ( icert );

	/* Allocate "private" key */
	icert->key = zalloc ( sizeof ( *icert->key ) );
	if ( ! icert->key ) {
		rc = -ENOMEM;
		goto error;
	}
	privkey_init ( icert->key );
	private = &icert->key->builder;

	/* Construct our "private" key */
	if ( ( rc = ( asn1_prepend_raw ( private, icert_private,
					 sizeof ( icert_private ) ),
		      asn1_prepend_raw ( private, icert_public,
					 sizeof ( icert_public ) ),
		      asn1_prepend ( private, ASN1_INTEGER, icert_nul,
				     sizeof ( icert_nul ) ),
		      asn1_wrap ( private, ASN1_SEQUENCE ) ) ) != 0 ) {
		DBGC ( icert, "ICERT %p could not build private key: %s\n",
		       icert, strerror ( rc ) );
		goto error;
	}

	/* Construct our own public key */
	if ( ( rc = ( asn1_prepend_raw ( &public, icert_public,
					 sizeof ( icert_public ) ),
		      asn1_wrap ( &public, ASN1_SEQUENCE ) ) ) != 0 ) {
		DBGC ( icert, "ICERT %p could not build public key: %s\n",
		       icert, strerror ( rc ) );
		goto error;
	}

	/* Construct root certificate */
	if ( ( rc = icert_cert ( icert, &icert_name_root, &icert_name_root,
				 asn1_built ( private ), asn1_built ( &public ),
				 &icert_root_exts, &icert->root ) ) != 0 )
		goto error;

	/* Construct host certificate */
	if ( ( rc = icert_cert ( icert, &icert_name_ipxe, &icert_name_root,
				 asn1_built ( private ), asn1_built ( &public ),
				 &icert_leaf_exts, &icert->host ) ) != 0 )
		goto error;

	/* Construct device certificate */
	if ( ( rc = icert_cert ( icert, &icert_name_iphone, &icert_name_root,
				 asn1_built ( private ), key,
				 &icert_leaf_exts, &icert->device ) ) != 0 )
		goto error;

	/* Construct root of trust */
	assert ( digest->digestsize == sizeof ( icert_root_fingerprint ) );
	x509_fingerprint ( icert->root, digest, icert_root_fingerprint );

	/* Free constructed keys */
	free ( public.data );
	return 0;

 error:
	icert_free ( icert );
	free ( public.data );
	return rc;
}

/**
 * Construct doubly base64-encoded certificate
 *
 * @v icert		Pairing certificates
 * @v cert		X.509 certificate
 * @v encenc		Doubly base64-encoded certificate to construct
 * @ret rc		Return status code
 *
 * On success, the caller is responsible for eventually calling free()
 * on the allocated doubly encoded encoded certificate.
 */
static int icert_encode ( struct icert *icert, struct x509_certificate *cert,
			  char **encenc ) {
	size_t encencoded_len;
	size_t encoded_len;
	size_t pem_len;
	char *pem;
	int rc;

	/* Sanity check */
	assert ( cert != NULL );

	/* Create PEM */
	encoded_len = ( base64_encoded_len ( cert->raw.len ) + 1 /* NUL */ );
	pem_len = ( ( sizeof ( icert_begin ) - 1 /* NUL */ ) +
		    ( encoded_len - 1 /* NUL */ ) +
		    ( sizeof ( icert_end ) - 1 /* NUL */ ) +
		    1 /* NUL */ );
	pem = malloc ( pem_len );
	if ( ! pem ) {
		rc = -ENOMEM;
		goto err_alloc_pem;
	}
	strcpy ( pem, icert_begin );
	base64_encode ( cert->raw.data, cert->raw.len,
			( pem + sizeof ( icert_begin ) - 1 /* NUL */ ),
			encoded_len );
	strcpy ( ( pem +
		   ( sizeof ( icert_begin ) - 1 /* NUL */ ) +
		   ( encoded_len - 1 /* NUL */ ) ), icert_end );
	DBGC2 ( icert, "ICERT %p \"%s\" certificate:\n%s",
		icert, x509_name ( cert ), pem );

	/* Base64-encode the PEM (sic) */
	encencoded_len = ( base64_encoded_len ( pem_len - 1 /* NUL */ )
			   + 1 /* NUL */ );
	*encenc = malloc ( encencoded_len );
	if ( ! *encenc ) {
		rc = -ENOMEM;
		goto err_alloc_encenc;
	}
	base64_encode ( pem, ( pem_len - 1 /* NUL */ ), *encenc,
			encencoded_len );

	/* Success */
	rc = 0;

 err_alloc_encenc:
	free ( pem );
 err_alloc_pem:
	return rc;
}

/******************************************************************************
 *
 * iPhone USB multiplexer
 *
 ******************************************************************************
 *
 * The iPhone USB multiplexer speaks a protocol that is almost, but
 * not quite, entirely unlike TCP.
 *
 */

/**
 * Transmit message
 *
 * @v imux		USB multiplexer
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int imux_tx ( struct imux *imux, struct io_buffer *iobuf ) {
	struct imux_header *hdr = iobuf->data;
	size_t len = iob_len ( iobuf );
	int rc;

	/* Populate header */
	assert ( len >= sizeof ( *hdr ) );
	hdr->len = htonl ( len );
	hdr->in_seq = htons ( imux->in_seq );
	hdr->out_seq = htons ( imux->out_seq );
	DBGCP ( imux, "IMUX %p transmitting:\n", imux );
	DBGCP_HDA ( imux, 0, hdr, len );

	/* Transmit message */
	if ( ( rc = usb_stream ( &imux->usbnet.out, iobuf, 1 ) ) != 0 )
		goto err;

	/* Increment sequence number */
	imux->out_seq++;

	return 0;

 err:
	free_iob ( iobuf );
	return rc;
}

/**
 * Transmit version message
 *
 * @v imux		USB multiplexer
 * @ret rc		Return status code
 */
static int imux_tx_version ( struct imux *imux ) {
	struct io_buffer *iobuf;
	struct imux_header_version *vers;
	int rc;

	/* Allocate I/O buffer */
	iobuf = alloc_iob ( sizeof ( *vers ) );
	if ( ! iobuf )
		return -ENOMEM;
	vers = iob_put ( iobuf, sizeof ( *vers ) );

	/* Construct version message */
	memset ( vers, 0, sizeof ( *vers ) );
	vers->hdr.protocol = htonl ( IMUX_VERSION );

	/* Transmit message */
	if ( ( rc = imux_tx ( imux, iob_disown ( iobuf ) ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Transmit pseudo-TCP message
 *
 * @v imux		USB multiplexer
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int imux_tx_tcp ( struct imux *imux, struct io_buffer *iobuf ) {
	struct imux_header_tcp *tcp = iobuf->data;
	size_t len = iob_len ( iobuf );
	int rc;

	/* Populate TCP header */
	assert ( len >= sizeof ( *tcp ) );
	tcp->hdr.protocol = htonl ( IMUX_TCP );
	tcp->tcp.src = htons ( imux->port );
	tcp->tcp.dest = htons ( IMUX_PORT_LOCKDOWND );
	tcp->tcp.seq = htonl ( imux->tcp_seq );
	tcp->tcp.ack = htonl ( imux->tcp_ack );
	tcp->tcp.hlen = ( ( sizeof ( tcp->tcp ) / 4 ) << 4 );
	tcp->tcp.win = htons ( IMUX_WINDOW );

	/* Transmit message */
	if ( ( rc = imux_tx ( imux, iob_disown ( iobuf ) ) ) != 0 )
		return rc;

	/* Update TCP sequence */
	imux->tcp_seq += ( len - sizeof ( *tcp ) );

	return 0;
}

/**
 * Transmit pseudo-TCP SYN
 *
 * @v imux		USB multiplexer
 * @ret rc		Return status code
 */
static int imux_tx_syn ( struct imux *imux ) {
	struct io_buffer *iobuf;
	struct imux_header_tcp *syn;
	int rc;

	/* Allocate I/O buffer */
	iobuf = alloc_iob ( sizeof ( *syn ) );
	if ( ! iobuf )
		return -ENOMEM;
	syn = iob_put ( iobuf, sizeof ( *syn ) );

	/* Construct TCP SYN message */
	memset ( syn, 0, sizeof ( *syn ) );
	syn->tcp.flags = TCP_SYN;

	/* Transmit message */
	if ( ( rc = imux_tx_tcp ( imux, iob_disown ( iobuf ) ) ) != 0 )
		return rc;

	/* Increment TCP sequence to compensate for SYN */
	imux->tcp_seq++;

	return 0;
}

/**
 * Open pairing client
 *
 * @v imux		USB multiplexer
 * @ret rc		Return status code
 */
static int imux_start_pair ( struct imux *imux ) {
	int rc;

	/* Disconnect any existing pairing client */
	intf_restart ( &imux->tcp, -EPIPE );

	/* Create pairing client */
	if ( ( rc = ipair_create ( &imux->tcp, imux->flags ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Receive version message
 *
 * @v imux		USB multiplexer
 */
static void imux_rx_version ( struct imux *imux ) {

	/* Reset output sequence */
	imux->out_seq = 0;

	/* Send TCP SYN */
	imux->action = imux_tx_syn;
}

/**
 * Receive log message
 *
 * @v imux		USB multiplexer
 * @v hdr		Message header
 * @v len		Length of message
 */
static void imux_rx_log ( struct imux *imux, struct imux_header *hdr,
			  size_t len ) {
	struct imux_header_log *log =
		container_of ( hdr, struct imux_header_log, hdr );
	unsigned int level;
	size_t msg_len;
	char *tmp;

	/* Sanity check */
	if ( len < sizeof ( *log ) ) {
		DBGC ( imux, "IMUX %p malformed log message:\n", imux );
		DBGC_HDA ( imux, 0, log, len );
		return;
	}

	/* First byte is the log level, followed by a printable
	 * message with no NUL terminator.  Extract the log level,
	 * then shuffle the message down within the buffer and append
	 * a NUL terminator.
	 */
	msg_len = ( len - sizeof ( *hdr ) );
	level = log->level;
	tmp = ( ( void * ) &log->level );
	memmove ( tmp, &log->msg, msg_len );
	tmp[msg_len] = '\0';

	/* Print log message */
	DBGC ( imux, "IMUX %p <%d>: %s\n", imux, level, tmp );
}

/**
 * Receive pseudo-TCP SYN+ACK
 *
 * @v imux		USB multiplexer
 */
static void imux_rx_syn ( struct imux *imux ) {

	/* Increment TCP acknowledgement to compensate for SYN */
	imux->tcp_ack++;

	/* Start pairing client */
	imux->action = imux_start_pair;
}

/**
 * Receive pseudo-TCP message
 *
 * @v imux		USB multiplexer
 * @v iobuf		I/O buffer
 */
static void imux_rx_tcp ( struct imux *imux, struct io_buffer *iobuf ) {
	struct imux_header_tcp *tcp = iobuf->data;
	size_t len = iob_len ( iobuf );
	int rc;

	/* Sanity check */
	if ( len < sizeof ( *tcp ) ) {
		DBGC ( imux, "IMUX %p malformed TCP message:\n", imux );
		DBGC_HDA ( imux, 0, tcp, len );
		goto error;
	}

	/* Ignore unexpected packets */
	if ( tcp->tcp.dest != htons ( imux->port ) ) {
		DBGC ( imux, "IMUX %p ignoring unexpected TCP port %d:\n",
		       imux, ntohs ( tcp->tcp.dest ) );
		DBGC_HDA ( imux, 0, tcp, len );
		goto error;
	}

	/* Ignore resets */
	if ( tcp->tcp.flags & TCP_RST ) {
		DBGC ( imux, "IMUX %p ignoring TCP RST\n", imux );
		DBGC2_HDA ( imux, 0, tcp, len );
		goto error;
	}

	/* Record ACK number */
	imux->tcp_ack = ( ntohl ( tcp->tcp.seq ) + len - sizeof ( *tcp ) );

	/* Handle received message */
	if ( tcp->tcp.flags & TCP_SYN ) {

		/* Received SYN+ACK */
		imux_rx_syn ( imux );

	} else {

		/* Strip header */
		iob_pull ( iobuf, sizeof ( *tcp ) );

		/* Deliver via socket */
		if ( ( rc = xfer_deliver_iob ( &imux->tcp,
					       iob_disown ( iobuf ) ) ) != 0 )
			goto error;
	}

 error:
	free_iob ( iobuf );
}

/**
 * Complete bulk IN transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void imux_in_complete ( struct usb_endpoint *ep,
			       struct io_buffer *iobuf, int rc ) {
	struct imux *imux = container_of ( ep, struct imux, usbnet.in );
	struct imux_header *hdr = iobuf->data;
	size_t len = iob_len ( iobuf );

	/* Ignore packets cancelled when the endpoint closes */
	if ( ! ep->open )
		goto drop;

	/* Report USB errors */
	if ( rc != 0 ) {
		DBGC ( imux, "IMUX %p bulk IN failed: %s\n",
		       imux, strerror ( rc ) );
		goto drop;
	}

	/* Sanity check */
	if ( len < sizeof ( *hdr ) ) {
		DBGC ( imux, "IMUX %p malformed message:\n", imux );
		DBGC_HDA ( imux, 0, hdr, len );
		goto drop;
	}

	/* Record input sequence */
	imux->in_seq = ntohs ( hdr->in_seq );

	/* Handle according to protocol */
	DBGCP ( imux, "IMUX %p received:\n", imux );
	DBGCP_HDA ( imux, 0, hdr, len );
	switch ( hdr->protocol ) {
	case htonl ( IMUX_VERSION ):
		imux_rx_version ( imux );
		break;
	case htonl ( IMUX_LOG ):
		imux_rx_log ( imux, hdr, len );
		break;
	case htonl ( IMUX_TCP ):
		imux_rx_tcp ( imux, iob_disown ( iobuf ) );
		break;
	default:
		DBGC ( imux, "IMUX %p unknown message type %d:\n",
		       imux, ntohl ( hdr->protocol ) );
		DBGC_HDA ( imux, 0, hdr, len );
		break;
	}

 drop:
	free_iob ( iobuf );
}

/** Bulk IN endpoint operations */
static struct usb_endpoint_driver_operations imux_in_operations = {
	.complete = imux_in_complete,
};

/**
 * Complete bulk OUT transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void imux_out_complete ( struct usb_endpoint *ep,
				struct io_buffer *iobuf, int rc ) {
	struct imux *imux = container_of ( ep, struct imux, usbnet.out );

	/* Report USB errors */
	if ( rc != 0 ) {
		DBGC ( imux, "IMUX %p bulk OUT failed: %s\n",
		       imux, strerror ( rc ) );
		goto error;
	}

 error:
	free_iob ( iobuf );
}

/** Bulk OUT endpoint operations */
static struct usb_endpoint_driver_operations imux_out_operations = {
	.complete = imux_out_complete,
};

/**
 * Shut down USB multiplexer
 *
 * @v imux		USB multiplexer
 */
static void imux_shutdown ( struct imux *imux ) {

	/* Shut down interfaces */
	intf_shutdown ( &imux->tcp, -ECANCELED );

	/* Close USB network device, if open */
	if ( process_running ( &imux->process ) ) {
		process_del ( &imux->process );
		usbnet_close ( &imux->usbnet );
	}
}

/**
 * Close USB multiplexer
 *
 * @v imux		USB multiplexer
 * @v rc		Reason for close
 */
static void imux_close ( struct imux *imux, int rc ) {
	struct iphone *iphone;

	/* Restart interfaces */
	intf_restart ( &imux->tcp, rc );

	/* Record pairing status */
	imux->rc = rc;

	/* Trigger link check on any associated iPhones */
	list_for_each_entry ( iphone, &iphones, list ) {
		if ( iphone->usb == imux->usb )
			start_timer_nodelay ( &iphone->timer );
	}

	/* Retry pairing on any error */
	if ( rc != 0 ) {

		/* Increment port number */
		imux->port++;

		/* Request pairing on any retry attempt */
		imux->flags = IPAIR_REQUEST;

		/* Send new pseudo-TCP SYN */
		imux->action = imux_tx_syn;

		DBGC ( imux, "IMUX %p retrying pairing: %s\n",
		       imux, strerror ( rc ) );
		return;
	}

	/* Shut down multiplexer on pairing success */
	imux_shutdown ( imux );
}

/**
 * Allocate I/O buffer for pseudo-TCP socket
 *
 * @v imux		USB multiplexer
 * @v len		I/O buffer payload length
 * @ret iobuf		I/O buffer
 */
static struct io_buffer * imux_alloc_iob ( struct imux *imux __unused,
					   size_t len ) {
	struct imux_header_tcp *tcp;
	struct io_buffer *iobuf;

	/* Allocate I/O buffer */
	iobuf = alloc_iob ( sizeof ( *tcp ) + len );
	if ( ! iobuf )
		return NULL;

	/* Reserve space for pseudo-TCP message header */
	iob_reserve ( iobuf, sizeof ( *tcp ) );

	return iobuf;
}

/**
 * Transmit packet via pseudo-TCP socket
 *
 * @v imux		USB multiplexer
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int imux_deliver ( struct imux *imux, struct io_buffer *iobuf,
			  struct xfer_metadata *meta __unused ) {
	struct imux_header_tcp *tcp;

	/* Prepend pseudo-TCP header */
	tcp = iob_push ( iobuf, sizeof ( *tcp ) );
	memset ( tcp, 0, sizeof ( *tcp ) );
	tcp->tcp.flags = TCP_ACK;

	/* Transmit pseudo-TCP packet */
	return imux_tx_tcp ( imux, iob_disown ( iobuf ) );
}

/** Pseudo-TCP socket interface operations */
static struct interface_operation imux_tcp_operations[] = {
	INTF_OP ( xfer_deliver, struct imux *, imux_deliver ),
	INTF_OP ( xfer_alloc_iob, struct imux *, imux_alloc_iob ),
	INTF_OP ( intf_close, struct imux *, imux_close ),
};

/** Pseudo-TCP socket interface descriptor */
static struct interface_descriptor imux_tcp_desc =
	INTF_DESC ( struct imux, tcp, imux_tcp_operations );

/**
 * Multiplexer process
 *
 * @v imux		USB multiplexer
 */
static void imux_step ( struct imux *imux ) {
	int rc;

	/* Poll USB bus */
	usb_poll ( imux->bus );

	/* Do nothing more if multiplexer has been closed */
	if ( ! process_running ( &imux->process ) )
		return;

	/* Refill endpoints */
	if ( ( rc = usbnet_refill ( &imux->usbnet ) ) != 0 ) {
		/* Wait for next poll */
		return;
	}

	/* Perform pending action, if any */
	if ( imux->action ) {
		if ( ( rc = imux->action ( imux ) ) != 0 )
			imux_close ( imux, rc );
		imux->action = NULL;
	}
}

/** Multiplexer process descriptor */
static struct process_descriptor imux_process_desc =
	PROC_DESC ( struct imux, process, imux_step );

/**
 * Probe device
 *
 * @v func		USB function
 * @v config		Configuration descriptor
 * @ret rc		Return status code
 */
static int imux_probe ( struct usb_function *func,
			struct usb_configuration_descriptor *config ) {
	struct usb_device *usb = func->usb;
	struct imux *imux;
	int rc;

	/* Allocate and initialise structure */
	imux = zalloc ( sizeof ( *imux ) );
	if ( ! imux ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	ref_init ( &imux->refcnt, NULL );
	imux->usb = usb;
	imux->bus = usb->port->hub->bus;
	usbnet_init ( &imux->usbnet, func, NULL, &imux_in_operations,
		      &imux_out_operations );
	usb_refill_init ( &imux->usbnet.in, 0, IMUX_IN_MTU, IMUX_IN_MAX_FILL );
	process_init ( &imux->process, &imux_process_desc, &imux->refcnt );
	imux->action = imux_tx_version;
	imux->port = IMUX_PORT_LOCAL;
	intf_init ( &imux->tcp, &imux_tcp_desc, &imux->refcnt );
	imux->rc = -EINPROGRESS_PAIRING;

	/* Describe USB network device */
	if ( ( rc = usbnet_describe ( &imux->usbnet, config ) ) != 0 ) {
		DBGC ( imux, "IMUX %p could not describe: %s\n",
		       imux, strerror ( rc ) );
		goto err_describe;
	}

	/* Open USB network device */
	if ( ( rc = usbnet_open ( &imux->usbnet ) ) != 0 ) {
		DBGC ( imux, "IMUX %p could not open: %s\n",
		       imux, strerror ( rc ) );
		goto err_open;
	}

	/* Start polling process */
	process_add ( &imux->process );

	/* Add to list of multiplexers */
	list_add ( &imux->list, &imuxes );

	usb_func_set_drvdata ( func, imux );
	return 0;

	list_del ( &imux->list );
	imux_shutdown ( imux );
 err_open:
 err_describe:
	ref_put ( &imux->refcnt );
 err_alloc:
	return rc;
}

/**
 * Remove device
 *
 * @v func		USB function
 */
static void imux_remove ( struct usb_function *func ) {
	struct imux *imux = usb_func_get_drvdata ( func );

	list_del ( &imux->list );
	imux_shutdown ( imux );
	ref_put ( &imux->refcnt );
}

/** USB multiplexer device IDs */
static struct usb_device_id imux_ids[] = {
	{
		.name = "imux",
		.vendor = 0x05ac,
		.product = USB_ANY_ID,
	},
};

/** USB multiplexer driver */
struct usb_driver imux_driver __usb_driver = {
	.ids = imux_ids,
	.id_count = ( sizeof ( imux_ids ) / sizeof ( imux_ids[0] ) ),
	.class = USB_CLASS_ID ( 0xff, 0xfe, 0x02 ),
	.score = USB_SCORE_NORMAL,
	.probe = imux_probe,
	.remove = imux_remove,
};

/******************************************************************************
 *
 * iPhone pairing client
 *
 ******************************************************************************
 */

/** Common prefix for all pairing messages */
static const char ipair_prefix[] =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
	"\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
	"<plist version=\"1.0\">\n"
	"<dict>\n"
	"<key>Label</key>\n"
	"<string>iPXE</string>\n"
	"<key>Request</key>\n";

/** Common suffix for all pairing messages */
static const char ipair_suffix[] =
	"</dict>\n"
	"</plist>\n";

/** Arbitrary system BUID used for pairing */
static const char ipair_system_buid[] = "E4DB92D2-248A-469A-AC34-92045D07E695";

/** Arbitrary host ID used for pairing */
static const char ipair_host_id[] = "93CEBC27-8457-4804-9108-F42549DF6143";

static int ipair_tx_pubkey ( struct ipair *ipair );
static int ipair_rx_pubkey ( struct ipair *ipair, char *msg );
static int ipair_tx_pair ( struct ipair *ipair );
static int ipair_rx_pair ( struct ipair *ipair, char *msg );
static int ipair_tx_session ( struct ipair *ipair );
static int ipair_rx_session ( struct ipair *ipair, char *msg );

/**
 * Free pairing client
 *
 * @v refcnt		Reference counter
 */
static void ipair_free ( struct refcnt *refcnt ) {
	struct ipair *ipair = container_of ( refcnt, struct ipair, refcnt );

	icert_free ( &ipair->icert );
	free ( ipair );
}

/**
 * Shut down pairing client
 *
 * @v ipair		Pairing client
 * @v rc		Reason for close
 */
static void ipair_close ( struct ipair *ipair, int rc ) {

	/* Shut down interfaces */
	intf_shutdown ( &ipair->xfer, rc );

	/* Stop timer */
	stop_timer ( &ipair->timer );
}

/**
 * Transmit XML message
 *
 * @v ipair		Pairing client
 * @v fmt		Format string
 * @v ...		Arguments
 * @ret rc		Return status code
 */
static int __attribute__ (( format ( printf, 2, 3 ) ))
ipair_tx ( struct ipair *ipair, const char *fmt, ... ) {
	struct io_buffer *iobuf;
	struct ipair_header *hdr;
	va_list args;
	size_t len;
	char *msg;
	int rc;

	/* Calculate length of formatted string */
	va_start ( args, fmt );
	len = ( vsnprintf ( NULL, 0, fmt, args ) + 1 /* NUL */ );
	va_end ( args );

	/* Allocate I/O buffer */
	iobuf = xfer_alloc_iob ( &ipair->xfer, ( sizeof ( *hdr ) + len ) );
	if ( ! iobuf )
		return -ENOMEM;
	hdr = iob_put ( iobuf, sizeof ( *hdr ) );

	/* Construct XML message */
	memset ( hdr, 0, sizeof ( *hdr ) );
	hdr->len = htonl ( len );
	msg = iob_put ( iobuf, len );
	va_start ( args, fmt );
	vsnprintf ( msg, len, fmt, args );
	va_end ( args );
	DBGC2 ( ipair, "IPAIR %p transmitting:\n%s\n", ipair, msg );

	/* Transmit message */
	if ( ( rc = xfer_deliver_iob ( &ipair->xfer,
				       iob_disown ( iobuf ) ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Receive XML message payload
 *
 * @v ipair		Pairing client
 * @v msg		Message payload
 * @v len		Length of message
 * @ret rc		Return status code
 */
static int ipair_rx ( struct ipair *ipair, char *msg, size_t len ) {
	int ( * rx ) ( struct ipair *ipair, char *msg );
	int rc;

	/* Ignore empty messages */
	if ( ! len )
		return 0;

	/* Sanity check */
	if ( ( msg[ len - 1 ] != '\0' ) && ( msg[ len - 1 ] != '\n' ) ) {
		DBGC ( ipair, "IPAIR %p malformed XML:\n", ipair );
		DBGC_HDA ( ipair, 0, msg, len );
		return -EPROTO;
	}

	/* Add NUL terminator (potentially overwriting final newline) */
	msg[ len - 1 ] = '\0';
	DBGC2 ( ipair, "IPAIR %p received:\n%s\n\n", ipair, msg );

	/* Handle according to current state */
	rx = ipair->rx;
	if ( ! rx ) {
		DBGC ( ipair, "IPAIR %p unexpected XML:\n%s\n", ipair, msg );
		return -EPROTO;
	}
	ipair->rx = NULL;
	if ( ( rc = rx ( ipair, msg ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Locate XML tag
 *
 * @v ipair		Pairing client
 * @v msg		XML message
 * @v tag		Tag name
 * @ret start		Start of tag content
 * @ret end		End of tag content
 * @ret rc		Return status code
 */
static int ipair_tag ( struct ipair *ipair, const char *msg, const char *tag,
		       char **start, char **end ) {
	char buf[ 2 /* "</" */ + strlen ( tag ) + 1 /* ">" */ + 1 /* NUL */ ];

	/* Locate opening tag */
	sprintf ( buf, "<%s>", tag );
	*start = strstr ( msg, buf );
	if ( ! *start )
		return -ENOENT;
	*start += strlen ( buf );

	/* Locate closing tag */
	sprintf ( buf, "</%s>", tag );
	*end = strstr ( *start, buf );
	if ( ! *end ) {
		DBGC ( ipair, "IPAIR %p missing closing tag %s in:\n%s\n",
		       ipair, buf, msg );
		return -ENOENT;
	}

	return 0;
}

/**
 * Locate XML property list dictionary value
 *
 * @v ipair		Pairing client
 * @v msg		XML message
 * @v key		Key name
 * @v type		Key type
 * @ret start		Start of value content
 * @ret end		End of value content
 * @ret rc		Return status code
 */
static int ipair_key ( struct ipair *ipair, const char *msg, const char *key,
		       const char *type, char **start, char **end ) {
	int rc;

	/* Iterate over keys */
	while ( 1 ) {

		/* Locate key */
		if ( ( rc = ipair_tag ( ipair, msg, "key", start,
					end ) ) != 0 )
			return rc;
		msg = *end;

		/* Check key name */
		if ( memcmp ( *start, key, ( *end - *start ) ) != 0 )
			continue;

		/* Locate value */
		return ipair_tag ( ipair, msg, type, start, end );
	}
}

/**
 * Transmit DevicePublicKey message
 *
 * @v ipair		Pairing client
 * @ret rc		Return status code
 */
static int ipair_tx_pubkey ( struct ipair *ipair ) {
	int rc;

	/* Transmit message */
	if ( ( rc = ipair_tx ( ipair,
			       "%s"
			       "<string>GetValue</string>\n"
			       "<key>Key</key>\n"
			       "<string>DevicePublicKey</string>\n"
			       "%s",
			       ipair_prefix, ipair_suffix ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Receive DevicePublicKey message
 *
 * @v ipair		Pairing client
 * @v msg		XML message
 * @ret rc		Return status code
 */
static int ipair_rx_pubkey ( struct ipair *ipair, char *msg ) {
	struct asn1_cursor *key;
	char *data;
	char *end;
	char *decoded;
	size_t max_len;
	int len;
	int next;
	int rc;

	/* Locate "Value" value */
	if ( ( rc = ipair_key ( ipair, msg, "Value", "data", &data,
				&end ) ) != 0 ) {
		DBGC ( ipair, "IPAIR %p unexpected public key message:\n%s\n",
		       ipair, msg );
		goto err_tag;
	}
	*end = '\0';

	/* Decode outer layer of Base64 */
	max_len = base64_decoded_max_len ( data );
	decoded = malloc ( max_len );
	if ( ! decoded ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	len = base64_decode ( data, decoded, max_len );
	if ( len < 0 ) {
		rc = len;
		DBGC ( ipair, "IPAIR %p invalid outer public key:\n%s\n",
		       ipair, data );
		goto err_decode;
	}

	/* Decode inner layer of Base64 */
	next = pem_asn1 ( decoded, len, 0, &key );
	if ( next < 0 ) {
		rc = next;
		DBGC ( ipair, "IPAIR %p invalid inner public key:\n%s\n",
		       ipair, decoded );
		goto err_asn1;
	}
	DBGC ( ipair, "IPAIR %p received public key\n", ipair );
	DBGC2_HDA ( ipair, 0, key->data, key->len );

	/* Construct certificates */
	if ( ( rc = icert_certs ( &ipair->icert, key ) ) != 0 )
		goto err_certs;

	/* Send session request or pair request as applicable */
	if ( ipair->flags & IPAIR_REQUEST ) {
		ipair->tx = ipair_tx_pair;
		ipair->rx = ipair_rx_pair;
	} else {
		ipair->tx = ipair_tx_session;
		ipair->rx = ipair_rx_session;
	}
	start_timer_nodelay ( &ipair->timer );

	/* Free key */
	free ( key );

	/* Free intermediate Base64 */
	free ( decoded );

	return 0;

 err_certs:
	free ( key );
 err_asn1:
 err_decode:
	free ( decoded );
 err_alloc:
 err_tag:
	return rc;
}

/**
 * Transmit Pair message
 *
 * @v ipair		Pairing client
 * @ret rc		Return status code
 */
static int ipair_tx_pair ( struct ipair *ipair ) {
	char *root;
	char *host;
	char *device;
	int rc;

	/* Construct doubly encoded certificates */
	if ( ( rc = icert_encode ( &ipair->icert, ipair->icert.root,
				   &root ) ) != 0 )
		goto err_root;
	if ( ( rc = icert_encode ( &ipair->icert, ipair->icert.host,
				   &host ) ) != 0 )
		goto err_host;
	if ( ( rc = icert_encode ( &ipair->icert, ipair->icert.device,
				   &device ) ) != 0 )
		goto err_device;

	/* Transmit message */
	if ( ( rc = ipair_tx ( ipair,
			       "%s"
			       "<string>Pair</string>\n"
			       "<key>PairRecord</key>\n"
			       "<dict>\n"
			       "<key>RootCertificate</key>\n"
			       "<data>%s</data>\n"
			       "<key>HostCertificate</key>\n"
			       "<data>%s</data>\n"
			       "<key>DeviceCertificate</key>\n"
			       "<data>%s</data>\n"
			       "<key>SystemBUID</key>\n"
			       "<string>%s</string>\n"
			       "<key>HostID</key>\n"
			       "<string>%s</string>\n"
			       "</dict>\n"
			       "<key>ProtocolVersion</key>\n"
			       "<string>2</string>\n"
			       "<key>PairingOptions</key>\n"
			       "<dict>\n"
			       "<key>ExtendedPairingErrors</key>\n"
			       "<true/>\n"
			       "</dict>\n"
			       "%s",
			       ipair_prefix, root, host, device,
			       ipair_system_buid, ipair_host_id,
			       ipair_suffix
			       ) ) != 0 )
		goto err_tx;

 err_tx:
	free ( device );
 err_device:
	free ( host );
 err_host:
	free ( root );
 err_root:
	return rc;
}

/**
 * Receive Pair message error
 *
 * @v ipair		Pairing client
 * @v error		Pairing error
 * @ret rc		Return status code
 */
static int ipair_rx_pair_error ( struct ipair *ipair, char *error ) {

	/* Check for actual errors */
	if ( strcmp ( error, "PairingDialogResponsePending" ) != 0 ) {
		DBGC ( ipair, "IPAIR %p pairing error \"%s\"\n", ipair, error );
		return -EPERM;
	}

	/* Retransmit pairing request */
	ipair->tx = ipair_tx_pair;
	ipair->rx = ipair_rx_pair;
	start_timer_fixed ( &ipair->timer, IPAIR_RETRY_DELAY );

	DBGC ( ipair, "IPAIR %p waiting for pairing dialog\n", ipair );
	return 0;
}

/**
 * Receive Pair message
 *
 * @v ipair		Pairing client
 * @v msg		XML message
 * @ret rc		Return status code
 */
static int ipair_rx_pair ( struct ipair *ipair, char *msg ) {
	char *error;
	char *escrow;
	char *end;
	int rc;

	/* Check for pairing errors */
	if ( ( rc = ipair_key ( ipair, msg, "Error", "string", &error,
				&end ) ) == 0 ) {
		*end = '\0';
		return ipair_rx_pair_error ( ipair, error );
	}

	/* Get EscrowBag */
	if ( ( rc = ipair_key ( ipair, msg, "EscrowBag", "data", &escrow,
				&end ) ) != 0 ) {
		DBGC ( ipair, "IPAIR %p unexpected pairing response:\n%s\n",
		       ipair, msg );
		return rc;
	}
	DBGC ( ipair, "IPAIR %p pairing successful\n", ipair );

	/* Send session request */
	ipair->tx = ipair_tx_session;
	ipair->rx = ipair_rx_session;
	start_timer_nodelay ( &ipair->timer );

	return 0;
}

/**
 * Transmit StartSession message
 *
 * @v ipair		Pairing client
 * @ret rc		Return status code
 */
static int ipair_tx_session ( struct ipair *ipair ) {
	int rc;

	/* Transmit message */
	if ( ( rc = ipair_tx ( ipair,
			       "%s"
			       "<string>StartSession</string>\n"
			       "<key>SystemBUID</key>\n"
			       "<string>%s</string>\n"
			       "<key>HostID</key>\n"
			       "<string>%s</string>\n"
			       "%s",
			       ipair_prefix, ipair_system_buid,
			       ipair_host_id, ipair_suffix
			       ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Receive StartSession message error
 *
 * @v ipair		Pairing client
 * @v error		Pairing error
 * @ret rc		Return status code
 */
static int ipair_rx_session_error ( struct ipair *ipair, char *error ) {

	/* Check for actual errors */
	if ( strcmp ( error, "InvalidHostID" ) != 0 ) {
		DBGC ( ipair, "IPAIR %p session error \"%s\"\n", ipair, error );
		return -EPERM;
	}

	/* Transmit pairing request */
	ipair->tx = ipair_tx_pair;
	ipair->rx = ipair_rx_pair;
	start_timer_nodelay ( &ipair->timer );

	DBGC ( ipair, "IPAIR %p unknown host: requesting pairing\n", ipair );
	return 0;
}

/**
 * Receive StartSession message
 *
 * @v ipair		Pairing client
 * @v msg		XML message
 * @ret rc		Return status code
 */
static int ipair_rx_session ( struct ipair *ipair, char *msg ) {
	char *error;
	char *session;
	char *end;
	int rc;

	/* Check for session errors */
	if ( ( rc = ipair_key ( ipair, msg, "Error", "string", &error,
				&end ) ) == 0 ) {
		*end = '\0';
		return ipair_rx_session_error ( ipair, error );
	}

	/* Check for session ID */
	if ( ( rc = ipair_key ( ipair, msg, "SessionID", "string", &session,
				&end ) ) != 0 ) {
		DBGC ( ipair, "IPAIR %p unexpected session response:\n%s\n",
		       ipair, msg );
		return rc;
	}
	*end = '\0';
	DBGC ( ipair, "IPAIR %p starting session \"%s\"\n", ipair, session );

	/* Start TLS */
	if ( ( rc = add_tls ( &ipair->xfer, "iPhone", &icert_root,
			      ipair->icert.key ) ) != 0 ) {
		DBGC ( ipair, "IPAIR %p could not start TLS: %s\n",
		       ipair, strerror ( rc ) );
		return rc;
	}

	/* Record that TLS has been started */
	ipair->flags |= IPAIR_TLS;

	return 0;
}

/**
 * Handle window change notification
 *
 * @v ipair		Pairing client
 */
static void ipair_window_changed ( struct ipair *ipair ) {

	/* Report pairing as complete once TLS session has been established */
	if ( ( ipair->flags & IPAIR_TLS ) && xfer_window ( &ipair->xfer ) ) {

		/* Sanity checks */
		assert ( x509_is_valid ( ipair->icert.root, &icert_root ) );
		assert ( x509_is_valid ( ipair->icert.device, &icert_root ) );
		assert ( ! x509_is_valid ( ipair->icert.root, NULL ) );
		assert ( ! x509_is_valid ( ipair->icert.host, NULL ) );
		assert ( ! x509_is_valid ( ipair->icert.device, NULL ) );

		/* Report pairing as complete */
		DBGC ( ipair, "IPAIR %p established TLS session\n", ipair );
		ipair_close ( ipair, 0 );
		return;
	}
}

/**
 * Handle received data
 *
 * @v ipair		Pairing client
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int ipair_deliver ( struct ipair *ipair, struct io_buffer *iobuf,
			   struct xfer_metadata *meta __unused ) {
	struct ipair_header *hdr;
	int rc;

	/* Strip header (which may appear in a separate packet) */
	if ( ( ! ( ipair->flags & IPAIR_RX_LEN ) ) &&
	     ( iob_len ( iobuf ) >= sizeof ( *hdr ) ) ) {
		iob_pull ( iobuf, sizeof ( *hdr ) );
		ipair->flags |= IPAIR_RX_LEN;
	}

	/* Clear received header flag if we have a message */
	if ( iob_len ( iobuf ) )
		ipair->flags &= ~IPAIR_RX_LEN;

	/* Receive message */
	if ( ( rc = ipair_rx ( ipair, iobuf->data, iob_len ( iobuf ) ) ) != 0 )
		goto error;

	/* Free I/O buffer */
	free_iob ( iobuf );

	return 0;

 error:
	ipair_close ( ipair, rc );
	free_iob ( iobuf );
	return rc;
}

/**
 * Pairing transmission timer
 *
 * @v timer		Retransmission timer
 * @v over		Failure indicator
 */
static void ipair_expired ( struct retry_timer *timer, int over __unused ) {
	struct ipair *ipair = container_of ( timer, struct ipair, timer );
	int ( * tx ) ( struct ipair *ipair );
	int rc;

	/* Sanity check */
	tx = ipair->tx;
	assert ( tx != NULL );

	/* Clear pending transmission */
	ipair->tx = NULL;

	/* Transmit data, if applicable */
	if ( ( rc = tx ( ipair ) ) != 0 )
		ipair_close ( ipair, rc );
}

/** Pairing client interface operations */
static struct interface_operation ipair_xfer_operations[] = {
	INTF_OP ( xfer_deliver, struct ipair *, ipair_deliver ),
	INTF_OP ( xfer_window_changed, struct ipair *, ipair_window_changed ),
	INTF_OP ( intf_close, struct ipair *, ipair_close ),
};

/** Pairing client interface descriptor */
static struct interface_descriptor ipair_xfer_desc =
	INTF_DESC ( struct ipair, xfer, ipair_xfer_operations );

/**
 * Create a pairing client
 *
 * @v xfer		Data transfer interface
 * @v flags		Initial state flags
 * @ret rc		Return status code
 */
static int ipair_create ( struct interface *xfer, unsigned int flags ) {
	struct ipair *ipair;
	int rc;

	/* Allocate and initialise structure */
	ipair = zalloc ( sizeof ( *ipair ) );
	if ( ! ipair ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	ref_init ( &ipair->refcnt, ipair_free );
	intf_init ( &ipair->xfer, &ipair_xfer_desc, &ipair->refcnt );
	timer_init ( &ipair->timer, ipair_expired, &ipair->refcnt );
	ipair->tx = ipair_tx_pubkey;
	ipair->rx = ipair_rx_pubkey;
	ipair->flags = flags;

	/* Schedule initial transmission */
	start_timer_nodelay ( &ipair->timer );

	/* Attach to parent interface, mortalise self, and return */
	intf_plug_plug ( &ipair->xfer, xfer );
	ref_put ( &ipair->refcnt );
	return 0;

	ref_put ( &ipair->refcnt );
 err_alloc:
	return rc;
}

/******************************************************************************
 *
 * iPhone USB networking
 *
 ******************************************************************************
 */

/**
 * Complete bulk IN transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void iphone_in_complete ( struct usb_endpoint *ep,
				 struct io_buffer *iobuf, int rc ) {
	struct iphone *iphone = container_of ( ep, struct iphone, usbnet.in );
	struct net_device *netdev = iphone->netdev;

	/* Profile receive completions */
	profile_start ( &iphone_in_profiler );

	/* Ignore packets cancelled when the endpoint closes */
	if ( ! ep->open )
		goto ignore;

	/* Record USB errors against the network device */
	if ( rc != 0 ) {
		DBGC ( iphone, "IPHONE %p bulk IN failed: %s\n",
		       iphone, strerror ( rc ) );
		goto error;
	}

	/* Strip padding */
	if ( iob_len ( iobuf ) < IPHONE_IN_PAD ) {
		DBGC ( iphone, "IPHONE %p malformed bulk IN:\n", iphone );
		DBGC_HDA ( iphone, 0, iobuf->data, iob_len ( iobuf ) );
		rc = -EINVAL;
		goto error;
	}
	iob_pull ( iobuf, IPHONE_IN_PAD );

	/* Hand off to network stack */
	netdev_rx ( netdev, iob_disown ( iobuf ) );

	profile_stop ( &iphone_in_profiler );
	return;

 error:
	netdev_rx_err ( netdev, iob_disown ( iobuf ), rc );
 ignore:
	free_iob ( iobuf );
}

/** Bulk IN endpoint operations */
static struct usb_endpoint_driver_operations iphone_in_operations = {
	.complete = iphone_in_complete,
};

/**
 * Transmit packet
 *
 * @v iphone		iPhone device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int iphone_out_transmit ( struct iphone *iphone,
				 struct io_buffer *iobuf ) {
	int rc;

	/* Profile transmissions */
	profile_start ( &iphone_out_profiler );

	/* Enqueue I/O buffer */
	if ( ( rc = usb_stream ( &iphone->usbnet.out, iobuf, 1 ) ) != 0 )
		return rc;

	profile_stop ( &iphone_out_profiler );
	return 0;
}

/**
 * Complete bulk OUT transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void iphone_out_complete ( struct usb_endpoint *ep,
				  struct io_buffer *iobuf, int rc ) {
	struct iphone *iphone = container_of ( ep, struct iphone, usbnet.out );
	struct net_device *netdev = iphone->netdev;

	/* Report TX completion */
	netdev_tx_complete_err ( netdev, iobuf, rc );
}

/** Bulk OUT endpoint operations */
static struct usb_endpoint_driver_operations iphone_out_operations = {
	.complete = iphone_out_complete,
};

/**
 * Check pairing status
 *
 * @v iphone		iPhone device
 * @ret rc		Return status code
 */
static int iphone_check_pair ( struct iphone *iphone ) {
	struct imux *imux;

	/* Find corresponding USB multiplexer */
	list_for_each_entry ( imux, &imuxes, list ) {
		if ( imux->usb == iphone->usb )
			return imux->rc;
	}

	return -EPIPE_NO_MUX;
}

/**
 * Check link status
 *
 * @v netdev		Network device
 */
static void iphone_check_link ( struct net_device *netdev ) {
	struct iphone *iphone = netdev->priv;
	struct usb_device *usb = iphone->usb;
	uint8_t status;
	int rc;

	/* Check pairing status */
	if ( ( rc = iphone_check_pair ( iphone ) ) != 0 )
		goto err_pair;

	/* Get link status */
	if ( ( rc = usb_control ( usb, IPHONE_GET_LINK, 0, 0, &status,
				  sizeof ( status ) ) ) != 0 ) {
		DBGC ( iphone, "IPHONE %p could not get link status: %s\n",
		       iphone, strerror ( rc ) );
		goto err_control;
	}

	/* Check link status */
	if ( status != IPHONE_LINK_UP ) {
		rc = -ENOTCONN_STATUS ( status );
		goto err_status;
	}

	/* Success */
	rc = 0;

 err_status:
 err_control:
 err_pair:
	/* Report link status.  Since we have to check the link
	 * periodically (due to an absence of an interrupt endpoint),
	 * do this only if the link status has actually changed.
	 */
	if ( rc != netdev->link_rc ) {
		if ( rc == 0 ) {
			DBGC ( iphone, "IPHONE %p link up\n", iphone );
		} else {
			DBGC ( iphone, "IPHONE %p link down: %s\n",
			       iphone, strerror ( rc ) );
		}
		netdev_link_err ( netdev, rc );
	}
}

/**
 * Periodically update link status
 *
 * @v timer		Link status timer
 * @v over		Failure indicator
 */
static void iphone_expired ( struct retry_timer *timer, int over __unused ) {
	struct iphone *iphone = container_of ( timer, struct iphone, timer );
	struct net_device *netdev = iphone->netdev;

	/* Check link status */
	iphone_check_link ( netdev );

	/* Restart timer, if device is open */
	if ( netdev_is_open ( netdev ) )
		start_timer_fixed ( timer, IPHONE_LINK_CHECK_INTERVAL );
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int iphone_open ( struct net_device *netdev ) {
	struct iphone *iphone = netdev->priv;
	int rc;

	/* Open USB network device */
	if ( ( rc = usbnet_open ( &iphone->usbnet ) ) != 0 ) {
		DBGC ( iphone, "IPHONE %p could not open: %s\n",
		       iphone, strerror ( rc ) );
		goto err_open;
	}

	/* Start the link status check timer */
	start_timer_nodelay ( &iphone->timer );

	return 0;

	usbnet_close ( &iphone->usbnet );
 err_open:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void iphone_close ( struct net_device *netdev ) {
	struct iphone *iphone = netdev->priv;

	/* Stop the link status check timer */
	stop_timer ( &iphone->timer );

	/* Close USB network device */
	usbnet_close ( &iphone->usbnet );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int iphone_transmit ( struct net_device *netdev,
			     struct io_buffer *iobuf ) {
	struct iphone *iphone = netdev->priv;
	int rc;

	/* Transmit packet */
	if ( ( rc = iphone_out_transmit ( iphone, iobuf ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void iphone_poll ( struct net_device *netdev ) {
	struct iphone *iphone = netdev->priv;
	int rc;

	/* Poll USB bus */
	usb_poll ( iphone->bus );

	/* Refill endpoints */
	if ( ( rc = usbnet_refill ( &iphone->usbnet ) ) != 0 )
		netdev_rx_err ( netdev, NULL, rc );
}

/** iPhone network device operations */
static struct net_device_operations iphone_operations = {
	.open		= iphone_open,
	.close		= iphone_close,
	.transmit	= iphone_transmit,
	.poll		= iphone_poll,
};

/**
 * Probe device
 *
 * @v func		USB function
 * @v config		Configuration descriptor
 * @ret rc		Return status code
 */
static int iphone_probe ( struct usb_function *func,
			  struct usb_configuration_descriptor *config ) {
	struct usb_device *usb = func->usb;
	struct net_device *netdev;
	struct iphone *iphone;
	int rc;

	/* Allocate and initialise structure */
	netdev = alloc_etherdev ( sizeof ( *iphone ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &iphone_operations );
	netdev->dev = &func->dev;
	iphone = netdev->priv;
	memset ( iphone, 0, sizeof ( *iphone ) );
	iphone->usb = usb;
	iphone->bus = usb->port->hub->bus;
	iphone->netdev = netdev;
	usbnet_init ( &iphone->usbnet, func, NULL, &iphone_in_operations,
		      &iphone_out_operations );
	usb_refill_init ( &iphone->usbnet.in, 0, IPHONE_IN_MTU,
			  IPHONE_IN_MAX_FILL );
	timer_init ( &iphone->timer, iphone_expired, &netdev->refcnt );
	DBGC ( iphone, "IPHONE %p on %s\n", iphone, func->name );

	/* Describe USB network device */
	if ( ( rc = usbnet_describe ( &iphone->usbnet, config ) ) != 0 ) {
		DBGC ( iphone, "IPHONE %p could not describe: %s\n",
		       iphone, strerror ( rc ) );
		goto err_describe;
	}

	/* Fetch MAC address */
	if ( ( rc = usb_control ( usb, IPHONE_GET_MAC, 0, 0, netdev->hw_addr,
				  ETH_ALEN ) ) != 0 ) {
		DBGC ( iphone, "IPHONE %p could not fetch MAC address: %s\n",
		       iphone, strerror ( rc ) );
		goto err_fetch_mac;
	}

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register;

	/* Set initial link status */
	iphone_check_link ( netdev );

	/* Add to list of iPhone network devices */
	list_add ( &iphone->list, &iphones );

	usb_func_set_drvdata ( func, iphone );
	return 0;

	list_del ( &iphone->list );
	unregister_netdev ( netdev );
 err_register:
 err_fetch_mac:
 err_describe:
	netdev_nullify ( netdev );
	netdev_put ( netdev );
 err_alloc:
	return rc;
}

/**
 * Remove device
 *
 * @v func		USB function
 */
static void iphone_remove ( struct usb_function *func ) {
	struct iphone *iphone = usb_func_get_drvdata ( func );
	struct net_device *netdev = iphone->netdev;

	list_del ( &iphone->list );
	unregister_netdev ( netdev );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** iPhone device IDs */
static struct usb_device_id iphone_ids[] = {
	{
		.name = "iphone",
		.vendor = 0x05ac,
		.product = USB_ANY_ID,
	},
};

/** iPhone driver */
struct usb_driver iphone_driver __usb_driver = {
	.ids = iphone_ids,
	.id_count = ( sizeof ( iphone_ids ) / sizeof ( iphone_ids[0] ) ),
	.class = USB_CLASS_ID ( 0xff, 0xfd, 0x01 ),
	.score = USB_SCORE_NORMAL,
	.probe = iphone_probe,
	.remove = iphone_remove,
};

/* Drag in objects via iphone_driver */
REQUIRING_SYMBOL ( iphone_driver );

/* Drag in RSA-with-SHA256 OID prefixes */
REQUIRE_OBJECT ( rsa_sha256 );
