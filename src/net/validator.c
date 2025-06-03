/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ipxe/refcnt.h>
#include <ipxe/malloc.h>
#include <ipxe/interface.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/iobuf.h>
#include <ipxe/xferbuf.h>
#include <ipxe/process.h>
#include <ipxe/x509.h>
#include <ipxe/settings.h>
#include <ipxe/dhcp.h>
#include <ipxe/base64.h>
#include <ipxe/crc32.h>
#include <ipxe/ocsp.h>
#include <ipxe/job.h>
#include <ipxe/validator.h>
#include <config/crypto.h>

/** @file
 *
 * Certificate validator
 *
 */

struct validator;

/** A certificate validator action */
struct validator_action {
	/** Name */
	const char *name;
	/** Action to take upon completed transfer */
	void ( * done ) ( struct validator *validator, int rc );
};

/** A certificate validator */
struct validator {
	/** Reference count */
	struct refcnt refcnt;
	/** Job control interface */
	struct interface job;
	/** Data transfer interface */
	struct interface xfer;

	/** Process */
	struct process process;
	/** Most relevant status code
	 *
	 * The cross-signed certificate mechanism may attempt several
	 * downloads as it works its way up the provided partial chain
	 * to locate a suitable cross-signed certificate with which to
	 * complete the chain.
	 *
	 * Some of these download or validation attempts may fail for
	 * uninteresting reasons (i.e. because a cross-signed
	 * certificate has never existed for that link in the chain).
	 *
	 * We must therefore keep track of the most relevant error
	 * that has occurred, in order to be able to report a
	 * meaningful overall status to the user.
	 *
	 * As a concrete example: consider the case of an expired OCSP
	 * signer for an intermediate certificate.  This will cause
	 * OCSP validation to fail for that intermediate certificate,
	 * and this is the error that should eventually be reported to
	 * the user.  We do not want to instead report the
	 * uninteresting fact that no cross-signed certificate was
	 * found for the remaining links in the chain, nor do we want
	 * to report just a generic "OCSP required" error.
	 *
	 * We record the most relevant status code whenever a
	 * definitely relevant error occurs, and clear it whenever we
	 * successfully make forward progress (e.g. by completing
	 * OCSP, or by adding new cross-signed certificates).
	 *
	 * When we subsequently attempt to validate the chain, we
	 * report the most relevant error status code (if recorded),
	 * otherwise we report the validation error itself.
	 */
	int rc;

	/** Root of trust (or NULL to use default) */
	struct x509_root *root;
	/** X.509 certificate chain */
	struct x509_chain *chain;
	/** OCSP check */
	struct ocsp_check *ocsp;
	/** Data buffer */
	struct xfer_buffer buffer;

	/** Current action */
	const struct validator_action *action;
	/** Current certificate (for progress reporting)
	 *
	 * This will always be present within the certificate chain
	 * and so this pointer does not hold a reference to the
	 * certificate.
	 */
	struct x509_certificate *cert;
	/** Current link within certificate chain */
	struct x509_link *link;
};

/**
 * Get validator name (for debug messages)
 *
 * @v validator		Certificate validator
 * @ret name		Validator name
 */
static const char * validator_name ( struct validator *validator ) {
	struct x509_certificate *cert;

	/* Use name of first certificate in chain, if present */
	cert = x509_first ( validator->chain );
	return ( cert ? x509_name ( cert ) : "<empty>" );
}

/**
 * Free certificate validator
 *
 * @v refcnt		Reference count
 */
static void validator_free ( struct refcnt *refcnt ) {
	struct validator *validator =
		container_of ( refcnt, struct validator, refcnt );

	DBGC2 ( validator, "VALIDATOR %p \"%s\" freed\n",
		validator, validator_name ( validator ) );
	x509_root_put ( validator->root );
	x509_chain_put ( validator->chain );
	ocsp_put ( validator->ocsp );
	xferbuf_free ( &validator->buffer );
	free ( validator );
}

/**
 * Mark certificate validation as finished
 *
 * @v validator		Certificate validator
 * @v rc		Reason for finishing
 */
static void validator_finished ( struct validator *validator, int rc ) {

	/* Remove process */
	process_del ( &validator->process );

	/* Close all interfaces */
	intf_shutdown ( &validator->xfer, rc );
	intf_shutdown ( &validator->job, rc );
}

/****************************************************************************
 *
 * Job control interface
 *
 */

/**
 * Report job progress
 *
 * @v validator		Certificate validator
 * @v progress		Progress report to fill in
 * @ret ongoing_rc	Ongoing job status code (if known)
 */
static int validator_progress ( struct validator *validator,
				struct job_progress *progress ) {

	/* Report current action, if applicable */
	if ( validator->action ) {
		snprintf ( progress->message, sizeof ( progress->message ),
			   "%s %s", validator->action->name,
			   x509_name ( validator->cert ) );
	}

	return 0;
}

/** Certificate validator job control interface operations */
static struct interface_operation validator_job_operations[] = {
	INTF_OP ( job_progress, struct validator *, validator_progress ),
	INTF_OP ( intf_close, struct validator *, validator_finished ),
};

/** Certificate validator job control interface descriptor */
static struct interface_descriptor validator_job_desc =
	INTF_DESC ( struct validator, job, validator_job_operations );

/****************************************************************************
 *
 * Cross-signing certificates
 *
 */

/** Cross-signed certificate source setting */
const struct setting crosscert_setting __setting ( SETTING_CRYPTO, crosscert )={
	.name = "crosscert",
	.description = "Cross-signed certificate source",
	.tag = DHCP_EB_CROSS_CERT,
	.type = &setting_type_string,
};

/** Default cross-signed certificate source */
static const char crosscert_default[] = CROSSCERT;

/**
 * Append cross-signing certificates to certificate chain
 *
 * @v validator		Certificate validator
 * @v rc		Completion status code
 * @ret rc		Return status code
 */
static void validator_append ( struct validator *validator, int rc ) {
	struct asn1_cursor cursor;
	struct x509_chain *certs;
	struct x509_certificate *cert;
	struct x509_link *link;
	struct x509_link *prev;

	/* Check for errors */
	if ( rc != 0 ) {
		DBGC ( validator, "VALIDATOR %p \"%s\" could not download ",
		       validator, validator_name ( validator ) );
		DBGC ( validator, "\"%s\" cross-signature: %s\n",
		       x509_name ( validator->cert ), strerror ( rc ) );
		/* If the overall validation is going to fail, then we
		 * will end up attempting multiple downloads for
		 * non-existent cross-signed certificates as we work
		 * our way up the certificate chain.  Do not record
		 * these as relevant errors, since we want to
		 * eventually report whichever much more relevant
		 * error occurred previously.
		 */
		goto err_irrelevant;
	}
	DBGC ( validator, "VALIDATOR %p \"%s\" downloaded ",
	       validator, validator_name ( validator ) );
	DBGC ( validator, "\"%s\" cross-signature\n",
	       x509_name ( validator->cert ) );

	/* Allocate certificate list */
	certs = x509_alloc_chain();
	if ( ! certs ) {
		rc = -ENOMEM;
		goto err_alloc_certs;
	}

	/* Initialise cursor */
	cursor.data = validator->buffer.data;
	cursor.len = validator->buffer.len;

	/* Enter certificateSet */
	if ( ( rc = asn1_enter ( &cursor, ASN1_SET ) ) != 0 ) {
		DBGC ( validator, "VALIDATOR %p \"%s\" could not enter "
		       "certificateSet: %s\n", validator,
		       validator_name ( validator ), strerror ( rc ) );
		goto err_certificateset;
	}

	/* Add each certificate to list */
	while ( cursor.len ) {

		/* Add certificate to list */
		if ( ( rc = x509_append_raw ( certs, cursor.data,
					      cursor.len ) ) != 0 ) {
			DBGC ( validator, "VALIDATOR %p \"%s\" could not "
			       "append certificate: %s\n", validator,
			       validator_name ( validator ), strerror ( rc) );
			DBGC_HDA ( validator, 0, cursor.data, cursor.len );
			goto err_append_raw;
		}
		cert = x509_last ( certs );
		DBGC ( validator, "VALIDATOR %p \"%s\" found certificate ",
		       validator, validator_name ( validator ) );
		DBGC ( validator, "%s\n", x509_name ( cert ) );

		/* Move to next certificate */
		asn1_skip_any ( &cursor );
	}

	/* Truncate existing certificate chain at current link */
	link = validator->link;
	assert ( link->flags & X509_LINK_FL_CROSSED );
	x509_truncate ( validator->chain, link );

	/* Append certificates to chain */
	if ( ( rc = x509_auto_append ( validator->chain, certs ) ) != 0 ) {
		DBGC ( validator, "VALIDATOR %p \"%s\" could not append "
		       "certificates: %s\n", validator,
		       validator_name ( validator ), strerror ( rc ) );
		goto err_auto_append;
	}

	/* Record that a cross-signed certificate download has already
	 * been performed for all but the last of the appended
	 * certificates.  (It may be necessary to perform a further
	 * download to complete the chain, if this download did not
	 * extend all the way to a root of trust.)
	 */
	prev = NULL;
	list_for_each_entry_continue ( link, &validator->chain->links, list ) {
		if ( prev )
			prev->flags |= X509_LINK_FL_CROSSED;
		prev = link;
	}

	/* Success */
	rc = 0;

 err_auto_append:
 err_append_raw:
 err_certificateset:
	x509_chain_put ( certs );
 err_alloc_certs:
	validator->rc = rc;
 err_irrelevant:
	/* Do not record irrelevant errors */
	return;
}

/** Cross-signing certificate download validator action */
static const struct validator_action validator_crosscert = {
	.name = "XCRT",
	.done = validator_append,
};

/**
 * Start download of cross-signing certificate
 *
 * @v validator		Certificate validator
 * @v link		Link in certificate chain
 * @ret rc		Return status code
 */
static int validator_start_download ( struct validator *validator,
				      struct x509_link *link ) {
	struct x509_certificate *cert = link->cert;
	const struct asn1_cursor *issuer = &cert->issuer.raw;
	const char *crosscert;
	char *crosscert_copy;
	char *uri_string;
	size_t uri_string_len;
	uint32_t crc;
	int len;
	int rc;

	/* Determine cross-signed certificate source */
	fetch_string_setting_copy ( NULL, &crosscert_setting, &crosscert_copy );
	crosscert = ( crosscert_copy ? crosscert_copy : crosscert_default );
	if ( ! crosscert[0] ) {
		rc = -EINVAL;
		goto err_check_uri_string;
	}

	/* Allocate URI string */
	uri_string_len = ( strlen ( crosscert ) + 22 /* "/%08x.der?subject=" */
			   + base64_encoded_len ( issuer->len ) + 1 /* NUL */ );
	uri_string = zalloc ( uri_string_len );
	if ( ! uri_string ) {
		rc = -ENOMEM;
		goto err_alloc_uri_string;
	}

	/* Generate CRC32 */
	crc = crc32_le ( 0xffffffffUL, issuer->data, issuer->len );

	/* Generate URI string */
	len = snprintf ( uri_string, uri_string_len, "%s/%08x.der?subject=",
			 crosscert, crc );
	base64_encode ( issuer->data, issuer->len, ( uri_string + len ),
			( uri_string_len - len ) );
	DBGC ( validator, "VALIDATOR %p \"%s\" downloading ",
	       validator, validator_name ( validator ) );
	DBGC ( validator, "\"%s\" cross-signature from %s\n",
	       x509_name ( cert ), uri_string );

	/* Set completion handler */
	validator->action = &validator_crosscert;
	validator->cert = cert;
	validator->link = link;

	/* Open URI */
	if ( ( rc = xfer_open_uri_string ( &validator->xfer,
					   uri_string ) ) != 0 ) {
		DBGC ( validator, "VALIDATOR %p \"%s\" could not open %s: "
		       "%s\n", validator, validator_name ( validator ),
		       uri_string, strerror ( rc ) );
		goto err_open_uri_string;
	}

	/* Free temporary allocations */
	free ( uri_string );
	free ( crosscert_copy );

	/* Success */
	return 0;

	intf_restart ( &validator->xfer, rc );
 err_open_uri_string:
	free ( uri_string );
 err_alloc_uri_string:
 err_check_uri_string:
	free ( crosscert_copy );
	validator->rc = rc;
	return rc;
}

/****************************************************************************
 *
 * OCSP checks
 *
 */

/**
 * Validate OCSP response
 *
 * @v validator		Certificate validator
 * @v rc		Completion status code
 */
static void validator_ocsp_validate ( struct validator *validator, int rc ) {
	const void *data = validator->buffer.data;
	size_t len = validator->buffer.len;
	time_t now;

	/* Check for errors */
	if ( rc != 0 ) {
		DBGC ( validator, "VALIDATOR %p \"%s\" could not fetch OCSP "
		       "response: %s\n", validator,
		       validator_name ( validator ), strerror ( rc ) );
		goto err_status;
	}

	/* Record OCSP response */
	if ( ( rc = ocsp_response ( validator->ocsp, data, len ) ) != 0 ) {
		DBGC ( validator, "VALIDATOR %p \"%s\" could not record OCSP "
		       "response: %s\n", validator,
		       validator_name ( validator ), strerror ( rc ) );
		goto err_response;
	}

	/* Validate OCSP response */
	now = time ( NULL );
	if ( ( rc = ocsp_validate ( validator->ocsp, now ) ) != 0 ) {
		DBGC ( validator, "VALIDATOR %p \"%s\" could not validate "
		       "OCSP response: %s\n", validator,
		       validator_name ( validator ), strerror ( rc ) );
		goto err_validate;
	}

	/* Success */
	DBGC ( validator, "VALIDATOR %p \"%s\" checked ",
	       validator, validator_name ( validator ) );
	DBGC ( validator, "\"%s\" via OCSP\n", x509_name ( validator->cert ) );

 err_validate:
 err_response:
 err_status:
	ocsp_put ( validator->ocsp );
	validator->ocsp = NULL;
	validator->rc = rc;
}

/** OCSP validator action */
static const struct validator_action validator_ocsp = {
	.name = "OCSP",
	.done = validator_ocsp_validate,
};

/**
 * Start OCSP check
 *
 * @v validator		Certificate validator
 * @v cert		Certificate to check
 * @v issuer		Issuing certificate
 * @ret rc		Return status code
 */
static int validator_start_ocsp ( struct validator *validator,
				  struct x509_certificate *cert,
				  struct x509_certificate *issuer ) {
	const char *uri_string;
	int rc;

	/* Create OCSP check */
	assert ( validator->ocsp == NULL );
	if ( ( rc = ocsp_check ( cert, issuer, &validator->ocsp ) ) != 0 ) {
		DBGC ( validator, "VALIDATOR %p \"%s\" could not create OCSP "
		       "check: %s\n", validator, validator_name ( validator ),
		       strerror ( rc ) );
		goto err_check;
	}

	/* Set completion handler */
	validator->action = &validator_ocsp;
	validator->cert = cert;

	/* Open URI */
	uri_string = validator->ocsp->uri_string;
	DBGC ( validator, "VALIDATOR %p \"%s\" checking ",
	       validator, validator_name ( validator ) );
	DBGC ( validator, "\"%s\" via %s\n",
	       x509_name ( cert ), uri_string );
	if ( ( rc = xfer_open_uri_string ( &validator->xfer,
					   uri_string ) ) != 0 ) {
		DBGC ( validator, "VALIDATOR %p \"%s\" could not open %s: "
		       "%s\n", validator, validator_name ( validator ),
		       uri_string, strerror ( rc ) );
		goto err_open;
	}

	return 0;

	intf_restart ( &validator->xfer, rc );
 err_open:
	ocsp_put ( validator->ocsp );
	validator->ocsp = NULL;
 err_check:
	validator->rc = rc;
	return rc;
}

/****************************************************************************
 *
 * Data transfer interface
 *
 */

/**
 * Close data transfer interface
 *
 * @v validator		Certificate validator
 * @v rc		Reason for close
 */
static void validator_xfer_close ( struct validator *validator, int rc ) {

	/* Close data transfer interface */
	intf_restart ( &validator->xfer, rc );
	DBGC2 ( validator, "VALIDATOR %p \"%s\" transfer complete\n",
		validator, validator_name ( validator ) );

	/* Process completed download */
	assert ( validator->action != NULL );
	validator->action->done ( validator, rc );

	/* Free downloaded data */
	xferbuf_free ( &validator->buffer );

	/* Resume validation process */
	process_add ( &validator->process );
}

/**
 * Receive data
 *
 * @v validator		Certificate validator
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int validator_xfer_deliver ( struct validator *validator,
				    struct io_buffer *iobuf,
				    struct xfer_metadata *meta ) {
	int rc;

	/* Add data to buffer */
	if ( ( rc = xferbuf_deliver ( &validator->buffer, iob_disown ( iobuf ),
				      meta ) ) != 0 ) {
		DBGC ( validator, "VALIDATOR %p \"%s\" could not receive "
		       "data: %s\n", validator, validator_name ( validator ),
		       strerror ( rc ) );
		validator_xfer_close ( validator, rc );
		return rc;
	}

	return 0;
}

/** Certificate validator data transfer interface operations */
static struct interface_operation validator_xfer_operations[] = {
	INTF_OP ( xfer_deliver, struct validator *, validator_xfer_deliver ),
	INTF_OP ( intf_close, struct validator *, validator_xfer_close ),
};

/** Certificate validator data transfer interface descriptor */
static struct interface_descriptor validator_xfer_desc =
	INTF_DESC ( struct validator, xfer, validator_xfer_operations );

/****************************************************************************
 *
 * Validation process
 *
 */

/**
 * Certificate validation process
 *
 * @v validator		Certificate validator
 */
static void validator_step ( struct validator *validator ) {
	struct x509_chain *chain = validator->chain;
	struct x509_link *link;
	struct x509_link *prev;
	struct x509_certificate *cert;
	time_t now;
	int rc;

	/* Try validating chain.  Try even if the chain is incomplete,
	 * since certificates may already have been validated
	 * previously.
	 */
	now = time ( NULL );
	if ( ( rc = x509_validate_chain ( chain, now, NULL,
					  validator->root ) ) == 0 ) {
		DBGC ( validator, "VALIDATOR %p \"%s\" validated\n",
		       validator, validator_name ( validator ) );
		validator_finished ( validator, 0 );
		return;
	}
	DBGC ( validator, "VALIDATOR %p \"%s\" not yet valid: %s\n",
	       validator, validator_name ( validator ), strerror ( rc ) );

	/* Record as the most relevant error, if no more relevant
	 * error has already been recorded.
	 */
	if ( validator->rc == 0 )
		validator->rc = rc;

	/* Find the first valid link in the chain, if any
	 *
	 * There is no point in attempting OCSP or cross-signed
	 * certificate downloads for certificates after the first
	 * valid link in the chain, since they cannot make a
	 * difference to the overall validation of the chain.
	 */
	prev = NULL;
	list_for_each_entry ( link, &chain->links, list ) {

		/* Dump link information (for debugging) */
		DBGC ( validator, "VALIDATOR %p \"%s\" has link ",
		       validator, validator_name ( validator ) );
		DBGC ( validator, "\"%s\"%s%s%s%s%s\n",
		       x509_name ( link->cert ),
		       ( ocsp_required ( link->cert ) ? " [NEEDOCSP]" : "" ),
		       ( ( link->flags & X509_LINK_FL_OCSPED ) ?
			 " [OCSPED]" : "" ),
		       ( ( link->flags & X509_LINK_FL_CROSSED ) ?
			 " [CROSSED]" : "" ),
		       ( x509_is_self_signed ( link->cert ) ? " [SELF]" : "" ),
		       ( x509_is_valid ( link->cert, validator->root ) ?
			 " [VALID]" : "" ) );

		/* Stop at first valid link */
		if ( x509_is_valid ( link->cert, validator->root ) )
			break;
		prev = link;
	}

	/* If this link is the issuer for a certificate that is
	 * pending an OCSP check attempt, then start OCSP to validate
	 * that certificate.
	 *
	 * If OCSP is not required for the issued certificate, or has
	 * already been attempted, or if we were unable to start OCSP
	 * for any reason, then proceed to attempting a cross-signed
	 * certificate download (which may end up replacing this
	 * issuer anyway).
	 */
	if ( ( ! list_is_head_entry ( link, &chain->links, list ) ) &&
	     ( ! ( link->flags & X509_LINK_FL_OCSPED ) ) &&
	     ( prev != NULL ) && ocsp_required ( prev->cert ) ) {

		/* Mark OCSP as attempted with this issuer */
		link->flags |= X509_LINK_FL_OCSPED;

		/* Start OCSP */
		if ( ( rc = validator_start_ocsp ( validator, prev->cert,
						   link->cert ) ) == 0 ) {
			/* Sleep until OCSP is complete */
			return;
		}
	}

	/* Work back up the chain (starting from the already
	 * identified first valid link, if any) to find a not-yet
	 * valid certificate for which we could attempt to download a
	 * cross-signed certificate chain.
	 */
	list_for_each_entry_continue_reverse ( link, &chain->links, list ) {
		cert = link->cert;

		/* Sanity check */
		assert ( ! x509_is_valid ( cert, validator->root ) );

		/* Skip self-signed certificates (cannot be cross-signed) */
		if ( x509_is_self_signed ( cert ) )
			continue;

		/* Skip previously attempted cross-signed downloads */
		if ( link->flags & X509_LINK_FL_CROSSED )
			continue;

		/* Mark cross-signed certificate download as attempted */
		link->flags |= X509_LINK_FL_CROSSED;

		/* Start cross-signed certificate download */
		if ( ( rc = validator_start_download ( validator,
						       link ) ) == 0 ) {
			/* Sleep until download is complete */
			return;
		}
	}

	/* Nothing more to try: fail the validation */
	validator_finished ( validator, validator->rc );
}

/** Certificate validator process descriptor */
static struct process_descriptor validator_process_desc =
	PROC_DESC_ONCE ( struct validator, process, validator_step );

/****************************************************************************
 *
 * Instantiator
 *
 */

/**
 * Instantiate a certificate validator
 *
 * @v job		Job control interface
 * @v chain		X.509 certificate chain
 * @v root		Root of trust, or NULL to use default
 * @ret rc		Return status code
 */
int create_validator ( struct interface *job, struct x509_chain *chain,
		       struct x509_root *root ) {
	struct validator *validator;
	int rc;

	/* Sanity check */
	if ( ! chain ) {
		rc = -EINVAL;
		goto err_sanity;
	}

	/* Allocate and initialise structure */
	validator = zalloc ( sizeof ( *validator ) );
	if ( ! validator ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	ref_init ( &validator->refcnt, validator_free );
	intf_init ( &validator->job, &validator_job_desc,
		    &validator->refcnt );
	intf_init ( &validator->xfer, &validator_xfer_desc,
		    &validator->refcnt );
	process_init ( &validator->process, &validator_process_desc,
		       &validator->refcnt );
	validator->root = x509_root_get ( root );
	validator->chain = x509_chain_get ( chain );
	xferbuf_malloc_init ( &validator->buffer );

	/* Attach parent interface, mortalise self, and return */
	intf_plug_plug ( &validator->job, job );
	ref_put ( &validator->refcnt );
	DBGC2 ( validator, "VALIDATOR %p \"%s\" validating X509 chain %p\n",
		validator, validator_name ( validator ), validator->chain );
	return 0;

	validator_finished ( validator, rc );
	ref_put ( &validator->refcnt );
 err_alloc:
 err_sanity:
	return rc;
}
