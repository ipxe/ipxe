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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Cryptographic Message Syntax (PKCS #7)
 *
 * The format of CMS messages is defined in RFC 5652.
 *
 */

#include <stdint.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ipxe/asn1.h>
#include <ipxe/x509.h>
#include <ipxe/image.h>
#include <ipxe/malloc.h>
#include <ipxe/privkey.h>
#include <ipxe/cms.h>

/* Disambiguate the various error causes */
#define EACCES_NON_SIGNING \
	__einfo_error ( EINFO_EACCES_NON_SIGNING )
#define EINFO_EACCES_NON_SIGNING \
	__einfo_uniqify ( EINFO_EACCES, 0x01, "Not a signing certificate" )
#define EACCES_NON_CODE_SIGNING \
	__einfo_error ( EINFO_EACCES_NON_CODE_SIGNING )
#define EINFO_EACCES_NON_CODE_SIGNING \
	__einfo_uniqify ( EINFO_EACCES, 0x02, "Not a code-signing certificate" )
#define EACCES_WRONG_NAME \
	__einfo_error ( EINFO_EACCES_WRONG_NAME )
#define EINFO_EACCES_WRONG_NAME \
	__einfo_uniqify ( EINFO_EACCES, 0x04, "Incorrect certificate name" )
#define EACCES_NO_SIGNATURES \
	__einfo_error ( EINFO_EACCES_NO_SIGNATURES )
#define EINFO_EACCES_NO_SIGNATURES \
	__einfo_uniqify ( EINFO_EACCES, 0x05, "No signatures present" )
#define EACCES_NO_RECIPIENTS \
	__einfo_error ( EINFO_EACCES_NO_RECIPIENTS )
#define EINFO_EACCES_NO_RECIPIENTS \
	__einfo_uniqify ( EINFO_EACCES, 0x06, "No usable recipients" )
#define EACCES_LEN \
	__einfo_error ( EINFO_EACCES_LEN )
#define EINFO_EACCES_LEN \
	__einfo_uniqify ( EINFO_EACCES, 0x07, "Bad file length" )
#define EACCES_PAD \
	__einfo_error ( EINFO_EACCES_PAD )
#define EINFO_EACCES_PAD \
	__einfo_uniqify ( EINFO_EACCES, 0x08, "Bad block padding" )
#define EACCES_MAC \
	__einfo_error ( EINFO_EACCES_MAC )
#define EINFO_EACCES_MAC \
	__einfo_uniqify ( EINFO_EACCES, 0x09, "Invalid MAC" )
#define ENOTSUP_TYPE \
	__einfo_error ( EINFO_ENOTSUP_TYPE )
#define EINFO_ENOTSUP_TYPE \
	__einfo_uniqify ( EINFO_ENOTSUP, 0x01, "Unrecognised message type" )

static int cms_parse_signed ( struct cms_message *cms,
			      const struct asn1_cursor *raw );
static int cms_parse_enveloped ( struct cms_message *cms,
				 const struct asn1_cursor *raw );

/** "id-signedData" object identifier */
static uint8_t oid_signeddata[] = { ASN1_OID_SIGNEDDATA };

/** "id-envelopedData" object identifier */
static uint8_t oid_envelopeddata[] = { ASN1_OID_ENVELOPEDDATA };

/** "id-authEnvelopedData" object identifier */
static uint8_t oid_authenvelopeddata[] = { ASN1_OID_AUTHENVELOPEDDATA };

/** CMS message types */
static struct cms_type cms_types[] = {
	{
		.name = "signed",
		.oid = ASN1_CURSOR ( oid_signeddata ),
		.parse = cms_parse_signed,
	},
	{
		.name = "enveloped",
		.oid = ASN1_CURSOR ( oid_envelopeddata ),
		.parse = cms_parse_enveloped,
	},
	{
		.name = "authEnveloped",
		.oid = ASN1_CURSOR ( oid_authenvelopeddata ),
		.parse = cms_parse_enveloped,
	}
};

/**
 * Parse CMS message content type
 *
 * @v cms		CMS message
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_content_type ( struct cms_message *cms,
				    const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	struct cms_type *type;
	unsigned int i;

	/* Enter contentType */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_OID );

	/* Check for a recognised OID */
	for ( i = 0 ; i < ( sizeof ( cms_types ) /
			    sizeof ( cms_types[0] ) ) ; i++ ) {
		type = &cms_types[i];
		if ( asn1_compare ( &cursor, &type->oid ) == 0 ) {
			cms->type = type;
			DBGC ( cms, "CMS %p contains %sData\n",
			       cms, type->name );
			return 0;
		}
	}

	DBGC ( cms, "CMS %p is not a recognised message type:\n", cms );
	DBGC_HDA ( cms, 0, raw->data, raw->len );
	return -ENOTSUP_TYPE;
}

/**
 * Parse CMS message certificate list
 *
 * @v cms		CMS message
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_certificates ( struct cms_message *cms,
				    const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	struct x509_certificate *cert;
	int rc;

	/* Enter certificates */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_EXPLICIT_TAG ( 0 ) );

	/* Add each certificate */
	while ( cursor.len ) {

		/* Add certificate to chain */
		if ( ( rc = x509_append_raw ( cms->certificates, cursor.data,
					      cursor.len ) ) != 0 ) {
			DBGC ( cms, "CMS %p could not append certificate: %s\n",
			       cms, strerror ( rc) );
			DBGC_HDA ( cms, 0, cursor.data, cursor.len );
			return rc;
		}
		cert = x509_last ( cms->certificates );
		DBGC ( cms, "CMS %p found certificate %s\n",
		       cms, x509_name ( cert ) );

		/* Move to next certificate */
		asn1_skip_any ( &cursor );
	}

	return 0;
}

/**
 * Parse CMS message participant identifier
 *
 * @v cms		CMS message
 * @v part		Participant information to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_identifier ( struct cms_message *cms,
				  struct cms_participant *part,
				  const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	struct asn1_cursor serial;
	struct asn1_cursor issuer;
	struct x509_certificate *cert;
	int rc;

	/* Enter issuerAndSerialNumber */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Identify issuer */
	memcpy ( &issuer, &cursor, sizeof ( issuer ) );
	if ( ( rc = asn1_shrink ( &issuer, ASN1_SEQUENCE ) ) != 0 ) {
		DBGC ( cms, "CMS %p/%p could not locate issuer: %s\n",
		       cms, part, strerror ( rc ) );
		DBGC_HDA ( cms, 0, raw->data, raw->len );
		return rc;
	}
	DBGC ( cms, "CMS %p/%p issuer is:\n", cms, part );
	DBGC_HDA ( cms, 0, issuer.data, issuer.len );
	asn1_skip_any ( &cursor );

	/* Identify serialNumber */
	memcpy ( &serial, &cursor, sizeof ( serial ) );
	if ( ( rc = asn1_shrink ( &serial, ASN1_INTEGER ) ) != 0 ) {
		DBGC ( cms, "CMS %p/%p could not locate serialNumber: %s\n",
		       cms, part, strerror ( rc ) );
		DBGC_HDA ( cms, 0, raw->data, raw->len );
		return rc;
	}
	DBGC ( cms, "CMS %p/%p serial number is:\n", cms, part );
	DBGC_HDA ( cms, 0, serial.data, serial.len );

	/* Identify certificate */
	cert = x509_find_issuer_serial ( cms->certificates, &issuer, &serial );
	if ( ! cert ) {
		DBGC ( cms, "CMS %p/%p could not identify certificate\n",
		       cms, part );
		return ( cms_is_signature ( cms ) ? -ENOENT : 0 );
	}

	/* Append certificate to chain */
	if ( ( rc = x509_append ( part->chain, cert ) ) != 0 ) {
		DBGC ( cms, "CMS %p/%p could not append certificate: %s\n",
		       cms, part, strerror ( rc ) );
		return rc;
	}

	/* Append remaining certificates to chain */
	if ( ( rc = x509_auto_append ( part->chain,
				       cms->certificates ) ) != 0 ) {
		DBGC ( cms, "CMS %p/%p could not append certificates: %s\n",
		       cms, part, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Parse CMS message digest algorithm
 *
 * @v cms		CMS message
 * @v part		Participant information to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_digest_algorithm ( struct cms_message *cms,
					struct cms_participant *part,
					const struct asn1_cursor *raw ) {
	struct asn1_algorithm *algorithm;
	int rc;

	/* Identify algorithm */
	if ( ( rc = asn1_digest_algorithm ( raw, &algorithm ) ) != 0 ) {
		DBGC ( cms, "CMS %p/%p could not identify digest algorithm: "
		       "%s\n", cms, part, strerror ( rc ) );
		DBGC_HDA ( cms, 0, raw->data, raw->len );
		return rc;
	}

	/* Record digest algorithm */
	part->digest = algorithm->digest;
	DBGC ( cms, "CMS %p/%p digest algorithm is %s\n",
	       cms, part, algorithm->name );

	return 0;
}

/**
 * Parse CMS message public-key algorithm
 *
 * @v cms		CMS message
 * @v part		Participant information to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_pubkey_algorithm ( struct cms_message *cms,
					struct cms_participant *part,
					const struct asn1_cursor *raw ) {
	struct asn1_algorithm *algorithm;
	int rc;

	/* Identify algorithm */
	if ( ( rc = asn1_pubkey_algorithm ( raw, &algorithm ) ) != 0 ) {
		DBGC ( cms, "CMS %p/%p could not identify public-key "
		       "algorithm: %s\n", cms, part, strerror ( rc ) );
		DBGC_HDA ( cms, 0, raw->data, raw->len );
		return rc;
	}

	/* Record public-key algorithm */
	part->pubkey = algorithm->pubkey;
	DBGC ( cms, "CMS %p/%p public-key algorithm is %s\n",
	       cms, part, algorithm->name );

	return 0;
}

/**
 * Parse CMS message cipher algorithm
 *
 * @v cms		CMS message
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_cipher_algorithm ( struct cms_message *cms,
					const struct asn1_cursor *raw ) {
	struct asn1_algorithm *algorithm;
	int rc;

	/* Identify algorithm */
	if ( ( rc = asn1_cipher_algorithm ( raw, &algorithm,
					    &cms->iv ) ) != 0 ) {
		DBGC ( cms, "CMS %p could not identify cipher algorithm: %s\n",
		       cms, strerror ( rc ) );
		DBGC_HDA ( cms, 0, raw->data, raw->len );
		return rc;
	}

	/* Record cipher */
	cms->cipher = algorithm->cipher;
	DBGC ( cms, "CMS %p cipher algorithm is %s\n", cms, algorithm->name );

	return 0;
}

/**
 * Parse CMS message signature or key value
 *
 * @v cms		CMS message
 * @v part		Participant information to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_value ( struct cms_message *cms,
			     struct cms_participant *part,
			     const struct asn1_cursor *raw ) {
	int rc;

	/* Enter signature or encryptedKey */
	memcpy ( &part->value, raw, sizeof ( part->value ) );
	if ( ( rc = asn1_enter ( &part->value, ASN1_OCTET_STRING ) ) != 0 ) {
		DBGC ( cms, "CMS %p/%p could not locate value:\n",
		       cms, part );
		DBGC_HDA ( cms, 0, raw->data, raw->len );
		return rc;
	}
	DBGC ( cms, "CMS %p/%p value is:\n", cms, part );
	DBGC_HDA ( cms, 0, part->value.data, part->value.len );

	return 0;
}

/**
 * Parse CMS message participant information
 *
 * @v cms		CMS message
 * @v part		Participant information to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_participant ( struct cms_message *cms,
				   struct cms_participant *part,
				   const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	int rc;

	/* Enter signerInfo or ktri */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Skip version */
	asn1_skip ( &cursor, ASN1_INTEGER );

	/* Parse sid or rid */
	if ( ( rc = cms_parse_identifier ( cms, part, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Parse signature-only objects */
	if ( cms_is_signature ( cms ) ) {

		/* Parse digestAlgorithm */
		if ( ( rc = cms_parse_digest_algorithm ( cms, part,
							 &cursor ) ) != 0 )
			return rc;
		asn1_skip_any ( &cursor );

		/* Skip signedAttrs, if present */
		asn1_skip_if_exists ( &cursor, ASN1_EXPLICIT_TAG ( 0 ) );
	}

	/* Parse signatureAlgorithm or contentEncryptionAlgorithm */
	if ( ( rc = cms_parse_pubkey_algorithm ( cms, part, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Parse signature or encryptedKey */
	if ( ( rc = cms_parse_value ( cms, part, &cursor ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Parse CMS message participants information
 *
 * @v cms		CMS message
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_participants ( struct cms_message *cms,
				    const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	struct cms_participant *part;
	int rc;

	/* Enter signerInfos or recipientInfos */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SET );

	/* Add each signerInfo or recipientInfo.  Errors are handled
	 * by ensuring that cms_put() will always be able to free any
	 * allocated memory.
	 */
	while ( cursor.len ) {

		/* Allocate participant information block */
		part = zalloc ( sizeof ( *part ) );
		if ( ! part )
			return -ENOMEM;
		list_add ( &part->list, &cms->participants );
		part->digest = &digest_null;
		part->pubkey = &pubkey_null;

		/* Allocate certificate chain */
		part->chain = x509_alloc_chain();
		if ( ! part->chain )
			return -ENOMEM;

		/* Parse signerInfo or recipientInfo */
		if ( ( rc = cms_parse_participant ( cms, part,
						    &cursor ) ) != 0 )
			return rc;
		asn1_skip_any ( &cursor );
	}

	return 0;
}

/**
 * Parse CMS message encrypted content information
 *
 * @v cms		CMS message
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_encrypted ( struct cms_message *cms,
				 const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	int rc;

	/* Enter encryptedContentInfo */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Skip contentType */
	asn1_skip ( &cursor, ASN1_OID );

	/* Parse contentEncryptionAlgorithm */
	if ( ( rc = cms_parse_cipher_algorithm ( cms, &cursor ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Parse CMS message MAC
 *
 * @v cms		CMS message
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_mac ( struct cms_message *cms,
			   const struct asn1_cursor *raw ) {
	int rc;

	/* Enter mac */
	memcpy ( &cms->mac, raw, sizeof ( cms->mac ) );
	if ( ( rc = asn1_enter ( &cms->mac, ASN1_OCTET_STRING ) ) != 0 ) {
		DBGC ( cms, "CMS %p could not locate mac: %s\n",
		       cms, strerror ( rc ) );
		DBGC_HDA ( cms, 0, raw->data, raw->len );
		return rc;
	}
	DBGC ( cms, "CMS %p mac is:\n", cms );
	DBGC_HDA ( cms, 0, cms->mac.data, cms->mac.len );

	return 0;
}

/**
 * Parse CMS signed data
 *
 * @v cms		CMS message
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_signed ( struct cms_message *cms,
			      const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	int rc;

	/* Allocate certificate list */
	cms->certificates = x509_alloc_chain();
	if ( ! cms->certificates )
		return -ENOMEM;

	/* Enter signedData */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Skip version */
	asn1_skip ( &cursor, ASN1_INTEGER );

	/* Skip digestAlgorithms */
	asn1_skip ( &cursor, ASN1_SET );

	/* Skip encapContentInfo */
	asn1_skip ( &cursor, ASN1_SEQUENCE );

	/* Parse certificates */
	if ( ( rc = cms_parse_certificates ( cms, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Skip crls, if present */
	asn1_skip_if_exists ( &cursor, ASN1_EXPLICIT_TAG ( 1 ) );

	/* Parse signerInfos */
	if ( ( rc = cms_parse_participants ( cms, &cursor ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Parse CMS enveloped data
 *
 * @v cms		CMS message
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_enveloped ( struct cms_message *cms,
				 const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	int rc;

	/* Enter envelopedData or authEnvelopedData */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Skip version */
	asn1_skip ( &cursor, ASN1_INTEGER );

	/* Skip originatorInfo, if present */
	asn1_skip_if_exists ( &cursor, ASN1_IMPLICIT_TAG ( 0 ) );

	/* Parse recipientInfos */
	if ( ( rc = cms_parse_participants ( cms, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Parse encryptedContentInfo or authEncryptedContentInfo */
	if ( ( rc = cms_parse_encrypted ( cms, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );
	assert ( cms->cipher != NULL );

	/* Skip unprotectedAttrs or authAttrs, if present */
	asn1_skip_if_exists ( &cursor, ASN1_IMPLICIT_TAG ( 1 ) );

	/* Parse mac, if present */
	if ( ( cms->cipher->authsize != 0 ) &&
	     ( ( rc = cms_parse_mac ( cms, &cursor ) ) != 0 ) )
		return rc;

	return 0;
}

/**
 * Parse CMS message from ASN.1 data
 *
 * @v cms		CMS message
 * @ret rc		Return status code
 */
static int cms_parse ( struct cms_message *cms ) {
	struct asn1_cursor cursor;
	int rc;

	/* Enter contentInfo */
	memcpy ( &cursor, cms->raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Parse contentType */
	if ( ( rc = cms_parse_content_type ( cms, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Enter content */
	asn1_enter ( &cursor, ASN1_EXPLICIT_TAG ( 0 ) );

	/* Parse type-specific content */
	if ( ( rc = cms->type->parse ( cms, &cursor ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Free CMS message
 *
 * @v refcnt		Reference count
 */
static void cms_free ( struct refcnt *refcnt ) {
	struct cms_message *cms =
		container_of ( refcnt, struct cms_message, refcnt );
	struct cms_participant *part;
	struct cms_participant *tmp;

	list_for_each_entry_safe ( part, tmp, &cms->participants, list ) {
		list_del ( &part->list );
		x509_chain_put ( part->chain );
		free ( part );
	}
	x509_chain_put ( cms->certificates );
	free ( cms->raw );
	free ( cms );
}

/**
 * Create CMS message
 *
 * @v image		Image
 * @ret sig		CMS message
 * @ret rc		Return status code
 *
 * On success, the caller holds a reference to the CMS message, and
 * is responsible for ultimately calling cms_put().
 */
int cms_message ( struct image *image, struct cms_message **cms ) {
	int next;
	int rc;

	/* Allocate and initialise message */
	*cms = zalloc ( sizeof ( **cms ) );
	if ( ! *cms ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	ref_init ( &(*cms)->refcnt, cms_free );
	INIT_LIST_HEAD ( &(*cms)->participants );
	(*cms)->cipher = &cipher_null;

	/* Get raw message data */
	next = image_asn1 ( image, 0, &(*cms)->raw );
	if ( next < 0 ) {
		rc = next;
		DBGC ( *cms, "CMS %p could not get raw ASN.1 data: %s\n",
		       *cms, strerror ( rc ) );
		goto err_asn1;
	}

	/* Use only first message in image */
	asn1_shrink_any ( (*cms)->raw );

	/* Parse message */
	if ( ( rc = cms_parse ( *cms ) ) != 0 )
		goto err_parse;

	return 0;

 err_parse:
 err_asn1:
	cms_put ( *cms );
 err_alloc:
	return rc;
}

/**
 * Calculate digest of CMS-signed data
 *
 * @v cms		CMS message
 * @v part		Participant information
 * @v data		Signed data
 * @v len		Length of signed data
 * @v out		Digest output
 */
static void cms_digest ( struct cms_message *cms,
			 struct cms_participant *part,
			 const void *data, size_t len, void *out ) {
	struct digest_algorithm *digest = part->digest;
	uint8_t ctx[ digest->ctxsize ];

	/* Calculate digest */
	digest_init ( digest, ctx );
	digest_update ( digest, ctx, data, len );
	digest_final ( digest, ctx, out );

	DBGC ( cms, "CMS %p/%p digest value:\n", cms, part );
	DBGC_HDA ( cms, 0, out, digest->digestsize );
}

/**
 * Verify digest of CMS-signed data
 *
 * @v cms		CMS message
 * @v part		Participant information
 * @v cert		Corresponding certificate
 * @v data		Signed data
 * @v len		Length of signed data
 * @ret rc		Return status code
 */
static int cms_verify_digest ( struct cms_message *cms,
			       struct cms_participant *part,
			       struct x509_certificate *cert,
			       const void *data, size_t len ) {
	struct digest_algorithm *digest = part->digest;
	struct pubkey_algorithm *pubkey = part->pubkey;
	const struct asn1_cursor *key = &cert->subject.public_key.raw;
	const struct asn1_cursor *value = &part->value;
	uint8_t digest_out[ digest->digestsize ];
	int rc;

	/* Generate digest */
	cms_digest ( cms, part, data, len, digest_out );

	/* Verify digest */
	if ( ( rc = pubkey_verify ( pubkey, key, digest, digest_out,
				    value->data, value->len ) ) != 0 ) {
		DBGC ( cms, "CMS %p/%p signature verification failed: %s\n",
		       cms, part, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Verify CMS message signer
 *
 * @v cms		CMS message
 * @v part		Participant information
 * @v data		Signed data
 * @v len		Length of signed data
 * @v time		Time at which to validate certificates
 * @v store		Certificate store, or NULL to use default
 * @v root		Root certificate list, or NULL to use default
 * @ret rc		Return status code
 */
static int cms_verify_signer ( struct cms_message *cms,
			       struct cms_participant *part,
			       const void *data, size_t len,
			       time_t time, struct x509_chain *store,
			       struct x509_root *root ) {
	struct x509_certificate *cert;
	int rc;

	/* Validate certificate chain */
	if ( ( rc = x509_validate_chain ( part->chain, time, store,
					  root ) ) != 0 ) {
		DBGC ( cms, "CMS %p/%p could not validate chain: %s\n",
		       cms, part, strerror ( rc ) );
		return rc;
	}

	/* Extract code-signing certificate */
	cert = x509_first ( part->chain );
	assert ( cert != NULL );

	/* Check that certificate can create digital signatures */
	if ( ! ( cert->extensions.usage.bits & X509_DIGITAL_SIGNATURE ) ) {
		DBGC ( cms, "CMS %p/%p certificate cannot create signatures\n",
		       cms, part );
		return -EACCES_NON_SIGNING;
	}

	/* Check that certificate can sign code */
	if ( ! ( cert->extensions.ext_usage.bits & X509_CODE_SIGNING ) ) {
		DBGC ( cms, "CMS %p/%p certificate is not code-signing\n",
		       cms, part );
		return -EACCES_NON_CODE_SIGNING;
	}

	/* Verify digest */
	if ( ( rc = cms_verify_digest ( cms, part, cert, data, len ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Verify CMS signature
 *
 * @v cms		CMS message
 * @v image		Signed image
 * @v name		Required common name, or NULL to check all signatures
 * @v time		Time at which to validate certificates
 * @v store		Certificate store, or NULL to use default
 * @v root		Root certificate list, or NULL to use default
 * @ret rc		Return status code
 */
int cms_verify ( struct cms_message *cms, struct image *image,
		 const char *name, time_t time, struct x509_chain *store,
		 struct x509_root *root ) {
	struct cms_participant *part;
	struct x509_certificate *cert;
	int count = 0;
	int rc;

	/* Mark image as untrusted */
	image_untrust ( image );

	/* Sanity check */
	if ( ! cms_is_signature ( cms ) )
		return -ENOTTY;

	/* Verify using all signers */
	list_for_each_entry ( part, &cms->participants, list ) {
		cert = x509_first ( part->chain );
		if ( name && ( x509_check_name ( cert, name ) != 0 ) )
			continue;
		if ( ( rc = cms_verify_signer ( cms, part, image->data,
						image->len, time, store,
						root ) ) != 0 )
			return rc;
		count++;
	}

	/* Check that we have verified at least one signature */
	if ( count == 0 ) {
		if ( name ) {
			DBGC ( cms, "CMS %p had no signatures matching name "
			       "%s\n", cms, name );
			return -EACCES_WRONG_NAME;
		} else {
			DBGC ( cms, "CMS %p had no signatures\n", cms );
			return -EACCES_NO_SIGNATURES;
		}
	}

	/* Mark image as trusted */
	image_trust ( image );

	return 0;
}

/**
 * Identify CMS recipient corresponding to private key
 *
 * @v cms		CMS message
 * @v private_key	Private key
 * @ret part		Participant information, or NULL if not found
 */
static struct cms_participant *
cms_recipient ( struct cms_message *cms, struct private_key *private_key ) {
	struct cms_participant *part;
	struct x509_certificate *cert;

	/* Identify certificate (if any) for which we have a private key */
	cert = x509_find_key ( NULL, private_key );
	if ( ! cert )
		return NULL;

	/* Identify corresponding recipient, if any */
	list_for_each_entry ( part, &cms->participants, list ) {
		if ( cert == x509_first ( part->chain ) )
			return part;
	}

	return NULL;
}

/**
 * Set CMS cipher key
 *
 * @v cms		CMS message
 * @v part		Participant information
 * @v private_key	Private key
 * @v ctx		Cipher context
 * @ret rc		Return status code
 */
static int cms_cipher_key ( struct cms_message *cms,
			    struct cms_participant *part,
			    struct private_key *private_key, void *ctx ) {
	struct cipher_algorithm *cipher = cms->cipher;
	struct pubkey_algorithm *pubkey = part->pubkey;
	const struct asn1_cursor *key = privkey_cursor ( private_key );
	const struct asn1_cursor *value = &part->value;
	size_t max_len = pubkey_max_len ( pubkey, key );
	uint8_t cipher_key[max_len];
	int len;
	int rc;

	/* Decrypt cipher key */
	len = pubkey_decrypt ( pubkey, key, value->data, value->len,
			       cipher_key );
	if ( len < 0 ) {
		rc = len;
		DBGC ( cms, "CMS %p/%p could not decrypt cipher key: %s\n",
		       cms, part, strerror ( rc ) );
		DBGC_HDA ( cms, 0, value->data, value->len );
		return rc;
	}
	DBGC ( cms, "CMS %p/%p cipher key:\n", cms, part );
	DBGC_HDA ( cms, 0, cipher_key, len );

	/* Set cipher key */
	if ( ( rc = cipher_setkey ( cipher, ctx, cipher_key, len ) ) != 0 ) {
		DBGC ( cms, "CMS %p could not set cipher key: %s\n",
		       cms, strerror ( rc ) );
		return rc;
	}

	/* Set cipher initialization vector */
	cipher_setiv ( cipher, ctx, cms->iv.data, cms->iv.len );
	if ( cms->iv.len ) {
		DBGC ( cms, "CMS %p cipher IV:\n", cms );
		DBGC_HDA ( cms, 0, cms->iv.data, cms->iv.len );
	}

	return 0;
}

/**
 * Initialise cipher for CMS decryption
 *
 * @v cms		CMS message
 * @v private_key	Private key
 * @v ctx		Cipher context
 * @ret rc		Return status code
 */
static int cms_cipher ( struct cms_message *cms,
			struct private_key *private_key, void *ctx ) {
	struct cms_participant *part;
	int rc;

	/* Identify a usable recipient */
	part = cms_recipient ( cms, private_key );
	if ( ! part ) {
		DBGC ( cms, "CMS %p had no usable recipients\n", cms );
		return -EACCES_NO_RECIPIENTS;
	}

	/* Decrypt and set cipher key */
	if ( ( rc = cms_cipher_key ( cms, part, private_key, ctx ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Check CMS padding
 *
 * @v cms		CMS message
 * @v data		Final block
 * @v len		Final block length
 * @ret len		Padding length, or negative error
 */
static int cms_verify_padding ( struct cms_message *cms, const void *data,
				size_t len ) {
	struct cipher_algorithm *cipher = cms->cipher;
	const uint8_t *pad;
	size_t pad_len;
	unsigned int i;

	/* Non-block ciphers do not use padding */
	if ( ! is_block_cipher ( cipher ) )
		return 0;

	/* Block padding can never produce an empty file */
	if ( len == 0 ) {
		DBGC ( cms, "CMS %p invalid empty padding\n", cms );
		return -EACCES_PAD;
	}

	/* Sanity check */
	assert ( len >= cipher->blocksize );

	/* Extract and verify padding */
	pad = ( data + len - 1 );
	pad_len = *pad;
	if ( ( pad_len == 0 ) || ( pad_len > len ) ) {
		DBGC ( cms, "CMS %p invalid padding length %zd\n",
		       cms, pad_len );
		return -EACCES_PAD;
	}
	for ( i = 0 ; i < pad_len ; i++ ) {
		if ( *(pad--) != pad_len ) {
			DBGC ( cms, "CMS %p invalid padding\n", cms );
			DBGC_HDA ( cms, 0, ( data + len - pad_len ), pad_len );
			return -EACCES_PAD;
		}
	}

	return pad_len;
}

/**
 * Decrypt CMS message
 *
 * @v cms		CMS message
 * @v image		Image to decrypt
 * @v name		Decrypted image name, or NULL to use default
 * @v private_key	Private key
 * @ret rc		Return status code
 */
int cms_decrypt ( struct cms_message *cms, struct image *image,
		  const char *name, struct private_key *private_key ) {
	struct cipher_algorithm *cipher = cms->cipher;
	const unsigned int original_flags = image->flags;
	uint8_t ctx[ cipher->ctxsize ];
	uint8_t ctxdup[ cipher->ctxsize ];
	uint8_t auth[ cipher->authsize ];
	uint8_t final[ cipher->blocksize ];
	size_t final_len;
	size_t bulk_len;
	int pad_len;
	int rc;

	/* Check block size */
	if ( ( image->len & ( cipher->blocksize - 1 ) ) != 0 ) {
		DBGC ( cms, "CMS %p invalid length %zd\n", cms, image->len );
		rc = -EACCES_LEN;
		goto err_blocksize;
	}

	/* Initialise cipher */
	if ( ( rc = cms_cipher ( cms, private_key, ctx ) ) != 0 )
		goto err_cipher;

	/* Duplicate cipher context for potential reencryption on error */
	memcpy ( ctxdup, ctx, cipher->ctxsize );

	/* Clear trusted flag before modifying image */
	image_untrust ( image );

	/* Temporarily unregister image, if applicable */
	if ( original_flags & IMAGE_REGISTERED ) {
		image_get ( image );
		unregister_image ( image );
	}

	/* Decrypt all but the final block */
	final_len = ( ( image->len && is_block_cipher ( cipher ) ) ?
		      cipher->blocksize : 0 );
	bulk_len = ( image->len - final_len );
	cipher_decrypt ( cipher, ctx, image->data, image->rwdata, bulk_len );

	/* Decrypt final block */
	cipher_decrypt ( cipher, ctx, ( image->data + bulk_len ), final,
			 final_len );

	/* Check authentication tag, if applicable */
	cipher_auth ( cipher, ctx, auth );
	if ( ( cms->mac.len != cipher->authsize ) ||
	     ( memcmp ( cms->mac.data, auth, cipher->authsize ) != 0 ) ) {
		DBGC ( cms, "CMS %p invalid authentication tag\n", cms );
		DBGC_HDA ( cms, 0, auth, cipher->authsize );
		rc = -EACCES_MAC;
		goto err_auth;
	}

	/* Check block padding, if applicable */
	if ( ( pad_len = cms_verify_padding ( cms, final, final_len ) ) < 0 ) {
		rc = pad_len;
		goto err_pad;
	}

	/* Update image name. Do this as the last possible failure, so
	 * that we do not have to include any error-handling code path
	 * to restore the original image name (which may itself fail).
	 */
	if ( name ) {
		if ( ( rc = image_set_name ( image, name ) ) != 0 )
			goto err_set_name;
	} else {
		image_strip_suffix ( image );
	}

	/* Overwrite final fragment and strip block padding.  Do this
	 * only once no further failure paths exist, so that we do not
	 * have to include include any error-handling code path to
	 * reconstruct the block padding.
	 */
	memcpy ( ( image->rwdata + bulk_len ), final, final_len );
	image->len -= pad_len;

	/* Clear image type and re-register image, if applicable */
	image->type = NULL;
	if ( original_flags & IMAGE_REGISTERED ) {
		register_image ( image );
		image_put ( image );
	}

	return 0;

 err_set_name:
 err_pad:
 err_auth:
	/* Reencrypt all overwritten portions.  This can be done since
	 * we have deliberately not overwritten the final block
	 * containing the potentially invalid (and therefore
	 * unreproducible) block padding.
	 */
	cipher_encrypt ( cipher, ctxdup, image->data, image->rwdata, bulk_len );
	if ( original_flags & IMAGE_REGISTERED ) {
		register_image ( image ); /* Cannot fail on re-registration */
		image_put ( image );
	}
	image->flags = original_flags;
 err_cipher:
 err_blocksize:
	return rc;
}
