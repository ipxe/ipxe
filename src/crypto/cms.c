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
#include <ipxe/uaccess.h>
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
#define ENOTSUP_TYPE \
	__einfo_error ( EINFO_ENOTSUP_TYPE )
#define EINFO_ENOTSUP_TYPE \
	__einfo_uniqify ( EINFO_ENOTSUP, 0x01, "Unrecognised message type" )

static int cms_parse_signed ( struct cms_message *cms,
			      const struct asn1_cursor *raw );

/** "id-signedData" object identifier */
static uint8_t oid_signeddata[] = { ASN1_OID_SIGNEDDATA };

/** CMS message types */
static struct cms_type cms_types[] = {
	{
		.name = "signed",
		.oid = ASN1_CURSOR ( oid_signeddata ),
		.parse = cms_parse_signed,
	},
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
		return -ENOENT;
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
	struct asn1_cursor cursor;
	int rc;

	/* Enter signature */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	if ( ( rc = asn1_enter ( &cursor, ASN1_OCTET_STRING ) ) != 0 ) {
		DBGC ( cms, "CMS %p/%p could not locate value:\n",
		       cms, part );
		DBGC_HDA ( cms, 0, raw->data, raw->len );
		return rc;
	}

	/* Record signature */
	part->len = cursor.len;
	part->value = malloc ( part->len );
	if ( ! part->value )
		return -ENOMEM;
	memcpy ( part->value, cursor.data, part->len );
	DBGC ( cms, "CMS %p/%p value is:\n", cms, part );
	DBGC_HDA ( cms, 0, part->value, part->len );

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

	/* Enter signerInfo */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Skip version */
	asn1_skip ( &cursor, ASN1_INTEGER );

	/* Parse sid */
	if ( ( rc = cms_parse_identifier ( cms, part, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Parse digestAlgorithm */
	if ( ( rc = cms_parse_digest_algorithm ( cms, part,
						 &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Skip signedAttrs, if present */
	asn1_skip_if_exists ( &cursor, ASN1_EXPLICIT_TAG ( 0 ) );

	/* Parse signatureAlgorithm */
	if ( ( rc = cms_parse_pubkey_algorithm ( cms, part, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Parse signature */
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

	/* Enter signerInfos */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SET );

	/* Add each signerInfo.  Errors are handled by ensuring that
	 * cms_put() will always be able to free any allocated memory.
	 */
	while ( cursor.len ) {

		/* Allocate participant information block */
		part = zalloc ( sizeof ( *part ) );
		if ( ! part )
			return -ENOMEM;
		list_add ( &part->list, &cms->participants );

		/* Allocate certificate chain */
		part->chain = x509_alloc_chain();
		if ( ! part->chain )
			return -ENOMEM;

		/* Parse signerInfo */
		if ( ( rc = cms_parse_participant ( cms, part,
						    &cursor ) ) != 0 )
			return rc;
		asn1_skip_any ( &cursor );
	}

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
 * Parse CMS message from ASN.1 data
 *
 * @v cms		CMS message
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse ( struct cms_message *cms,
		       const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	int rc;

	/* Enter contentInfo */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
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
		free ( part->value );
		free ( part );
	}
	x509_chain_put ( cms->certificates );
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
	struct asn1_cursor *raw;
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

	/* Get raw message data */
	next = image_asn1 ( image, 0, &raw );
	if ( next < 0 ) {
		rc = next;
		DBGC ( *cms, "CMS %p could not get raw ASN.1 data: %s\n",
		       *cms, strerror ( rc ) );
		goto err_asn1;
	}

	/* Use only first message in image */
	asn1_shrink_any ( raw );

	/* Parse message */
	if ( ( rc = cms_parse ( *cms, raw ) ) != 0 )
		goto err_parse;

	/* Free raw message data */
	free ( raw );

	return 0;

 err_parse:
	free ( raw );
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
			 userptr_t data, size_t len, void *out ) {
	struct digest_algorithm *digest = part->digest;
	uint8_t ctx[ digest->ctxsize ];
	uint8_t block[ digest->blocksize ];
	size_t offset = 0;
	size_t frag_len;

	/* Initialise digest */
	digest_init ( digest, ctx );

	/* Process data one block at a time */
	while ( len ) {
		frag_len = len;
		if ( frag_len > sizeof ( block ) )
			frag_len = sizeof ( block );
		copy_from_user ( block, data, offset, frag_len );
		digest_update ( digest, ctx, block, frag_len );
		offset += frag_len;
		len -= frag_len;
	}

	/* Finalise digest */
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
			       userptr_t data, size_t len ) {
	struct digest_algorithm *digest = part->digest;
	struct pubkey_algorithm *pubkey = part->pubkey;
	struct asn1_cursor *key = &cert->subject.public_key.raw;
	uint8_t digest_out[ digest->digestsize ];
	int rc;

	/* Generate digest */
	cms_digest ( cms, part, data, len, digest_out );

	/* Verify digest */
	if ( ( rc = pubkey_verify ( pubkey, key, digest, digest_out,
				    part->value, part->len ) ) != 0 ) {
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
			       userptr_t data, size_t len,
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
