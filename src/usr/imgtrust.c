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

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <syslog.h>
#include <ipxe/uaccess.h>
#include <ipxe/image.h>
#include <ipxe/cms.h>
#include <ipxe/base16.h>
#include <ipxe/validator.h>
#include <ipxe/monojob.h>
#include <usr/imgtrust.h>

/** @file
 *
 * Image trust management
 *
 */

/**
 * Verify image using downloaded signature
 *
 * @v image		Image to verify
 * @v signature		Image containing signature
 * @v name		Required common name, or NULL to allow any name
 * @ret rc		Return status code
 */
int imgverify ( struct image *image, struct image *signature,
		const char *name ) {
	struct asn1_cursor *data;
	struct cms_signature *sig;
	struct cms_signer_info *info;
	time_t now;
	int next;
	int rc;

	/* Mark image as untrusted */
	image_untrust ( image );

	/* Get raw signature data */
	next = image_asn1 ( signature, 0, &data );
	if ( next < 0 ) {
		rc = next;
		goto err_asn1;
	}

	/* Parse signature */
	if ( ( rc = cms_signature ( data->data, data->len, &sig ) ) != 0 )
		goto err_parse;

	/* Free raw signature data */
	free ( data );
	data = NULL;

	/* Complete all certificate chains */
	list_for_each_entry ( info, &sig->info, list ) {
		if ( ( rc = create_validator ( &monojob, info->chain,
					       NULL ) ) != 0 )
			goto err_create_validator;
		if ( ( rc = monojob_wait ( NULL, 0 ) ) != 0 )
			goto err_validator_wait;
	}

	/* Use signature to verify image */
	now = time ( NULL );
	if ( ( rc = cms_verify ( sig, image->data, image->len,
				 name, now, NULL, NULL ) ) != 0 )
		goto err_verify;

	/* Drop reference to signature */
	cms_put ( sig );
	sig = NULL;

	/* Mark image as trusted */
	image_trust ( image );
	syslog ( LOG_NOTICE, "Image \"%s\" signature OK\n", image->name );

	return 0;

 err_verify:
 err_validator_wait:
 err_create_validator:
	cms_put ( sig );
 err_parse:
	free ( data );
 err_asn1:
	syslog ( LOG_ERR, "Image \"%s\" signature bad: %s\n",
		 image->name, strerror ( rc ) );
	return rc;
}


/**
 * Calculate digest of user data
 *
 * @v digest		Digest alogorithm
 * @v data		Data to digest
 * @v len		Length of data
 * @v out		Digest output
 */
static void digest_user_data ( struct digest_algorithm *digest, userptr_t data,
			       size_t len, void *out ) {
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
}


/**
 * Identify a digest algorithm by name
 *
 * @v name		Digest name

 * @ret digest		Digest algorithm, or NULL
 */
static struct digest_algorithm *
find_digest_algorithm ( const char *name ) {
	struct asn1_algorithm *algorithm;

	for_each_table_entry ( algorithm, ASN1_ALGORITHMS ) {
		if ( strcmp ( algorithm->name, name ) == 0
		     && algorithm->digest )
			return algorithm->digest;
	}

	return NULL;
}

/**
 * Verify image using the supplied digest
 *
 * @v image		Image to verify
 * @v digest_name	Name of digest algorithm to use
 * @v hex		Hex encoded digest
 * @ret rc		Return status code
 */
int imgverifydigest ( struct image *image, const char *digest_name,
		      const char *hex ) {
	struct digest_algorithm *digest;
	uint8_t in[ base16_decoded_max_len ( hex ) ];
	uint8_t out[ base16_decoded_max_len ( hex ) ];
	int rc;

	/* Mark image as untrusted */
	image_untrust ( image );

	/* Parse digest name */
	if ( ! ( digest = find_digest_algorithm ( digest_name ) ) ) {
		syslog ( LOG_ERR, "Invalid digest name: %s\n", digest_name );
		rc = -EINVAL;
		goto err_verify;
	}

	/* Parse hex input digest */
	if ( base16_decode ( hex, in ) != (int) digest->digestsize )  {
		syslog ( LOG_ERR, "Invalid digest: %s %s\n", digest_name, hex );
		rc = -EINVAL;
		goto err_verify;
	}

	/* Verify digest */
	digest_user_data ( digest, image->data, image->len, out );
	if ( memcmp ( in, out, digest->digestsize ) != 0 )  {
		rc = -EINVAL;
		goto err_verify;
	}

	/* Mark image as trusted */
	image_trust ( image );
	syslog ( LOG_NOTICE, "Image \"%s\" digest OK\n", image->name );

	return 0;

 err_verify:
	syslog ( LOG_ERR, "Image \"%s\" digest bad: %s\n",
		 image->name, strerror ( rc ) );
	return rc;
}
