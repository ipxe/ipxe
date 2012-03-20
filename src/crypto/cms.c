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
#define EACCES_INCOMPLETE \
	__einfo_error ( EINFO_EACCES_INCOMPLETE )
#define EINFO_EACCES_INCOMPLETE \
	__einfo_uniqify ( EINFO_EACCES, 0x03, "Incomplete certificate chain" )
#define EACCES_WRONG_NAME \
	__einfo_error ( EINFO_EACCES_WRONG_NAME )
#define EINFO_EACCES_WRONG_NAME \
	__einfo_uniqify ( EINFO_EACCES, 0x04, "Incorrect certificate name" )

/** "pkcs7-signedData" object identifier */
static uint8_t oid_signeddata[] = { ASN1_OID_SIGNEDDATA };

/** "pkcs7-signedData" object identifier cursor */
static struct asn1_cursor oid_signeddata_cursor =
	ASN1_OID_CURSOR ( oid_signeddata );

/**
 * Parse CMS signature content type
 *
 * @v sig		CMS signature
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_content_type ( struct cms_signature *sig,
				    const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;

	/* Enter contentType */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_OID );

	/* Check OID is pkcs7-signedData */
	if ( asn1_compare ( &cursor, &oid_signeddata_cursor ) != 0 ) {
		DBGC ( sig, "CMS %p does not contain signedData:\n", sig );
		DBGC_HDA ( sig, 0, raw->data, raw->len );
		return -ENOTSUP;
	}

	DBGC ( sig, "CMS %p contains signedData\n", sig );
	return 0;
}

/**
 * Parse CMS signature signer identifier
 *
 * @v sig		CMS signature
 * @v info		Signer information to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_signer_identifier ( struct cms_signature *sig,
					 struct cms_signer_info *info,
					 const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	int rc;

	/* Enter issuerAndSerialNumber */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Record issuer */
	memcpy ( &info->issuer, &cursor, sizeof ( info->issuer ) );
	if ( ( rc = asn1_shrink ( &info->issuer, ASN1_SEQUENCE ) ) != 0 ) {
		DBGC ( sig, "CMS %p/%p could not locate issuer: %s\n",
		       sig, info, strerror ( rc ) );
		DBGC_HDA ( sig, 0, raw->data, raw->len );
		return rc;
	}
	DBGC ( sig, "CMS %p/%p issuer is:\n", sig, info );
	DBGC_HDA ( sig, 0, info->issuer.data, info->issuer.len );
	asn1_skip_any ( &cursor );

	/* Record serialNumber */
	memcpy ( &info->serial, &cursor, sizeof ( info->serial ) );
	if ( ( rc = asn1_shrink ( &info->serial, ASN1_INTEGER ) ) != 0 ) {
		DBGC ( sig, "CMS %p/%p could not locate serialNumber: %s\n",
		       sig, info, strerror ( rc ) );
		DBGC_HDA ( sig, 0, raw->data, raw->len );
		return rc;
	}
	DBGC ( sig, "CMS %p/%p serial number is:\n", sig, info );
	DBGC_HDA ( sig, 0, info->serial.data, info->serial.len );

	return 0;
}

/**
 * Parse CMS signature digest algorithm
 *
 * @v sig		CMS signature
 * @v info		Signer information to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_digest_algorithm ( struct cms_signature *sig,
					struct cms_signer_info *info,
					const struct asn1_cursor *raw ) {
	struct asn1_algorithm *algorithm;

	/* Identify algorithm */
	algorithm = asn1_algorithm ( raw );
	if ( ! algorithm ) {
		DBGC ( sig, "CMS %p/%p could not identify digest algorithm:\n",
		       sig, info );
		DBGC_HDA ( sig, 0, raw->data, raw->len );
		return -ENOTSUP;
	}

	/* Check algorithm is a digest algorithm */
	if ( ! algorithm->digest ) {
		DBGC ( sig, "CMS %p/%p algorithm %s is not a digest "
		       "algorithm\n", sig, info, algorithm->name );
		return -EINVAL;
	}

	/* Record digest algorithm */
	info->digest = algorithm->digest;
	DBGC ( sig, "CMS %p/%p digest algorithm is %s\n",
	       sig, info, algorithm->name );

	return 0;
}

/**
 * Parse CMS signature algorithm
 *
 * @v sig		CMS signature
 * @v info		Signer information to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_signature_algorithm ( struct cms_signature *sig,
					   struct cms_signer_info *info,
					   const struct asn1_cursor *raw ) {
	struct asn1_algorithm *algorithm;

	/* Identify algorithm */
	algorithm = asn1_algorithm ( raw );
	if ( ! algorithm ) {
		DBGC ( sig, "CMS %p/%p could not identify public-key "
		       "algorithm:\n", sig, info );
		DBGC_HDA ( sig, 0, raw->data, raw->len );
		return -ENOTSUP;
	}

	/* Check algorithm is a signature algorithm */
	if ( ! algorithm->pubkey ) {
		DBGC ( sig, "CMS %p/%p algorithm %s is not a public-key "
		       "algorithm\n", sig, info, algorithm->name );
		return -EINVAL;
	}

	/* Record signature algorithm */
	info->pubkey = algorithm->pubkey;
	DBGC ( sig, "CMS %p/%p public-key algorithm is %s\n",
	       sig, info, algorithm->name );

	return 0;
}

/**
 * Parse CMS signature value
 *
 * @v sig		CMS signature
 * @v info		Signer information to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_signature_value ( struct cms_signature *sig,
				       struct cms_signer_info *info,
				       const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	int rc;

	/* Enter signature */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	if ( ( rc = asn1_enter ( &cursor, ASN1_OCTET_STRING ) ) != 0 ) {
		DBGC ( sig, "CMS %p/%p could not locate signature:\n",
		       sig, info );
		DBGC_HDA ( sig, 0, raw->data, raw->len );
		return rc;
	}

	/* Record signature */
	info->signature = cursor.data;
	info->signature_len = cursor.len;
	DBGC ( sig, "CMS %p/%p signature value is:\n", sig, info );
	DBGC_HDA ( sig, 0, info->signature, info->signature_len );

	return 0;
}

/**
 * Parse CMS signature signer information
 *
 * @v sig		CMS signature
 * @v info		Signer information to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int cms_parse_signer_info ( struct cms_signature *sig,
				   struct cms_signer_info *info,
				   const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	int rc;

	/* Enter signerInfo */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Skip version */
	asn1_skip ( &cursor, ASN1_INTEGER );

	/* Parse sid */
	if ( ( rc = cms_parse_signer_identifier ( sig, info, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Parse digestAlgorithm */
	if ( ( rc = cms_parse_digest_algorithm ( sig, info, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Skip signedAttrs, if present */
	asn1_skip_if_exists ( &cursor, ASN1_EXPLICIT_TAG ( 0 ) );

	/* Parse signatureAlgorithm */
	if ( ( rc = cms_parse_signature_algorithm ( sig, info, &cursor ) ) != 0)
		return rc;
	asn1_skip_any ( &cursor );

	/* Parse signature */
	if ( ( rc = cms_parse_signature_value ( sig, info, &cursor ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Parse CMS signature from ASN.1 data
 *
 * @v sig		CMS signature
 * @v data		Raw signature data
 * @v len		Length of raw data
 * @ret rc		Return status code
 */
int cms_parse ( struct cms_signature *sig, const void *data, size_t len ) {
	struct asn1_cursor cursor;
	int rc;

	/* Initialise signature */
	memset ( sig, 0, sizeof ( *sig ) );
	cursor.data = data;
	cursor.len = len;

	/* Enter contentInfo */
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Parse contentType */
	if ( ( rc = cms_parse_content_type ( sig, &cursor ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Enter content */
	asn1_enter ( &cursor, ASN1_EXPLICIT_TAG ( 0 ) );

	/* Enter signedData */
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Skip version */
	asn1_skip ( &cursor, ASN1_INTEGER );

	/* Skip digestAlgorithms */
	asn1_skip ( &cursor, ASN1_SET );

	/* Skip encapContentInfo */
	asn1_skip ( &cursor, ASN1_SEQUENCE );

	/* Record certificates */
	memcpy ( &sig->certificates, &cursor, sizeof ( sig->certificates ) );
	if ( ( rc = asn1_enter ( &sig->certificates,
				 ASN1_EXPLICIT_TAG ( 0 ) ) ) != 0 ) {
		DBGC ( sig, "CMS %p could not locate certificates:\n", sig );
		DBGC_HDA ( sig, 0, data, len );
		return rc;
	}
	asn1_skip_any ( &cursor );

	/* Skip crls, if present */
	asn1_skip_if_exists ( &cursor, ASN1_EXPLICIT_TAG ( 1 ) );

	/* Enter signerInfos */
	asn1_enter ( &cursor, ASN1_SET );

	/* Parse first signerInfo */
	if ( ( rc = cms_parse_signer_info ( sig, &sig->info, &cursor ) ) != 0 )
		return rc;

	return 0;
}

/** CMS certificate chain context */
struct cms_chain_context {
	/** Signature */
	struct cms_signature *sig;
	/** Signer information */
	struct cms_signer_info *info;
};

/**
 * Parse next certificate in chain
 *
 * @v cert		X.509 certificate to parse
 * @v previous		Previous X.509 certificate, or NULL
 * @v ctx		Chain context
 * @ret rc		Return status code
 */
static int cms_parse_next ( struct x509_certificate *cert,
			    const struct x509_certificate *previous,
			    void *ctx ) {
	struct cms_chain_context *context = ctx;
	struct cms_signature *sig = context->sig;
	struct cms_signer_info *info = context->info;
	struct asn1_cursor cursor;
	int rc;

	/* Search for relevant certificate */
	memcpy ( &cursor, &sig->certificates, sizeof ( cursor ) );
	while ( cursor.len ) {

		/* Parse certificate */
		if ( ( rc = x509_parse ( cert, cursor.data,
					 cursor.len ) ) != 0 ) {
			DBGC ( sig, "CMS %p/%p could not parse certificate:\n",
			       sig, info );
			DBGC_HDA ( sig, 0, cursor.data, cursor.len );
			return rc;
		}

		if ( previous == NULL ) {
			/* First certificate: check issuer and serial
			 * number against signer info
			 */
			if ( ( asn1_compare ( &info->issuer,
					      &cert->issuer.raw ) == 0 ) &&
			     ( asn1_compare ( &info->serial,
					      &cert->serial.raw ) == 0 ) ) {
				return 0;
			}
		} else {
			/* Subsequent certificates: check subject
			 * against previous certificate's issuer.
			 */
			if ( asn1_compare ( &previous->issuer.raw,
					    &cert->subject.raw ) == 0 ) {
				return 0;
			}
		}

		/* Move to next certificate */
		asn1_skip_any ( &cursor );
	}

	DBGC ( sig, "CMS %p/%p reached end of certificate chain\n", sig, info );
	return -EACCES_INCOMPLETE;
}

/**
 * Calculate digest of CMS-signed data
 *
 * @v sig		CMS signature
 * @v info		Signer information
 * @v data		Signed data
 * @v len		Length of signed data
 * @v out		Digest output
 */
static void cms_digest ( struct cms_signature *sig,
			 struct cms_signer_info *info,
			 userptr_t data, size_t len, void *out ) {
	struct digest_algorithm *digest = info->digest;
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

	DBGC ( sig, "CMS %p/%p digest value:\n", sig, info );
	DBGC_HDA ( sig, 0, out, digest->digestsize );
}

/**
 * Verify digest of CMS-signed data
 *
 * @v sig		CMS signature
 * @v info		Signer information
 * @v cert		Corresponding certificate
 * @v data		Signed data
 * @v len		Length of signed data
 * @ret rc		Return status code
 */
static int cms_verify_digest ( struct cms_signature *sig,
			       struct cms_signer_info *info,
			       struct x509_certificate *cert,
			       userptr_t data, size_t len ) {
	struct digest_algorithm *digest = info->digest;
	struct pubkey_algorithm *pubkey = info->pubkey;
	struct x509_public_key *public_key = &cert->subject.public_key;
	uint8_t digest_out[ digest->digestsize ];
	uint8_t ctx[ pubkey->ctxsize ];
	int rc;

	/* Generate digest */
	cms_digest ( sig, info, data, len, digest_out );

	/* Initialise public-key algorithm */
	if ( ( rc = pubkey_init ( pubkey, ctx, public_key->raw.data,
				  public_key->raw.len ) ) != 0 ) {
		DBGC ( sig, "CMS %p/%p could not initialise public key: %s\n",
		       sig, info, strerror ( rc ) );
		goto err_init;
	}

	/* Verify digest */
	if ( ( rc = pubkey_verify ( pubkey, ctx, digest, digest_out,
				    info->signature,
				    info->signature_len ) ) != 0 ) {
		DBGC ( sig, "CMS %p/%p signature verification failed: %s\n",
		       sig, info, strerror ( rc ) );
		return rc;
	}

	pubkey_final ( pubkey, ctx );
 err_init:
	return rc;
}

/**
 * Verify CMS signature signer information
 *
 * @v sig		CMS signature
 * @v info		Signer information
 * @v data		Signed data
 * @v len		Length of signed data
 * @v name		Required common name, or NULL to allow any name
 * @v time		Time at which to validate certificates
 * @v root		Root certificate store, or NULL to use default
 * @ret rc		Return status code
 */
static int cms_verify_signer_info ( struct cms_signature *sig,
				    struct cms_signer_info *info,
				    userptr_t data, size_t len,
				    const char *name, time_t time,
				    struct x509_root *root ) {
	struct cms_chain_context context;
	struct x509_certificate cert;
	int rc;

	/* Validate certificate chain */
	context.sig = sig;
	context.info = info;
	if ( ( rc = x509_validate_chain ( cms_parse_next, &context, time, root,
					  &cert ) ) != 0 ) {
		DBGC ( sig, "CMS %p/%p could not validate chain: %s\n",
		       sig, info, strerror ( rc ) );
		return rc;
	}

	/* Check that certificate can create digital signatures */
	if ( ! ( cert.extensions.usage.bits & X509_DIGITAL_SIGNATURE ) ) {
		DBGC ( sig, "CMS %p/%p certificate cannot create signatures\n",
		       sig, info );
		return -EACCES_NON_SIGNING;
	}

	/* Check that certificate can sign code */
	if ( ! ( cert.extensions.ext_usage.bits & X509_CODE_SIGNING ) ) {
		DBGC ( sig, "CMS %p/%p certificate is not code-signing\n",
		       sig, info );
		return -EACCES_NON_CODE_SIGNING;
	}

	/* Check certificate name, if applicable */
	if ( ( name != NULL ) &&
	     ( ( cert.subject.name.len != strlen ( name ) ) ||
	       ( memcmp ( cert.subject.name.data, name,
			  cert.subject.name.len ) != 0 ) ) ) {
		DBGC ( sig, "CMS %p/%p certificate name incorrect\n",
		       sig, info );
		return -EACCES_WRONG_NAME;
	}

	/* Verify digest */
	if ( ( rc = cms_verify_digest ( sig, info, &cert, data, len ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Verify CMS signature
 *
 * @v sig		CMS signature
 * @v data		Signed data
 * @v len		Length of signed data
 * @v name		Required common name, or NULL to allow any name
 * @v time		Time at which to validate certificates
 * @v root		Root certificate store, or NULL to use default
 * @ret rc		Return status code
 */
int cms_verify ( struct cms_signature *sig, userptr_t data, size_t len,
		 const char *name, time_t time, struct x509_root *root ) {
	int rc;

	/* Verify using first signerInfo */
	if ( ( rc = cms_verify_signer_info ( sig, &sig->info, data, len,
					     name, time, root ) ) != 0 )
		return rc;

	return 0;
}
