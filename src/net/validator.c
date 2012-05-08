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
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <string.h>
#include <errno.h>
#include <ipxe/refcnt.h>
#include <ipxe/malloc.h>
#include <ipxe/interface.h>
#include <ipxe/process.h>
#include <ipxe/x509.h>
#include <ipxe/validator.h>

/** @file
 *
 * Certificate validator
 *
 */

/** A certificate validator */
struct validator {
	/** Reference count */
	struct refcnt refcnt;
	/** Job control interface */
	struct interface job;
	/** Process */
	struct process process;
	/** X.509 certificate chain */
	struct x509_chain *chain;
};

/**
 * Free certificate validator
 *
 * @v refcnt		Reference count
 */
static void validator_free ( struct refcnt *refcnt ) {
	struct validator *validator =
		container_of ( refcnt, struct validator, refcnt );

	DBGC ( validator, "VALIDATOR %p freed\n", validator );
	x509_chain_put ( validator->chain );
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
	intf_shutdown ( &validator->job, rc );
}

/****************************************************************************
 *
 * Job control interface
 *
 */

/** Certificate validator job control interface operations */
static struct interface_operation validator_job_operations[] = {
	INTF_OP ( intf_close, struct validator *, validator_finished ),
};

/** Certificate validator job control interface descriptor */
static struct interface_descriptor validator_job_desc =
	INTF_DESC ( struct validator, job, validator_job_operations );

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
	time_t now;
	int rc;

	/* Attempt to validate certificate chain */
	now = time ( NULL );
	if ( ( rc = x509_validate_chain ( validator->chain, now,
					  NULL ) ) != 0 ) {
		DBGC ( validator, "VALIDATOR %p could not validate chain: %s\n",
		       validator, strerror ( rc ) );
		goto err_validate;
	}

	/* Mark validation as complete */
	validator_finished ( validator, 0 );

	return;

 err_validate:
	validator_finished ( validator, rc );
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
 * @ret rc		Return status code
 */
int create_validator ( struct interface *job, struct x509_chain *chain ) {
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
	process_init ( &validator->process, &validator_process_desc,
		       &validator->refcnt );
	validator->chain = x509_chain_get ( chain );

	/* Attach parent interface, mortalise self, and return */
	intf_plug_plug ( &validator->job, job );
	ref_put ( &validator->refcnt );
	DBGC ( validator, "VALIDATOR %p validating X509 chain %p\n",
	       validator, validator->chain );
	return 0;

	validator_finished ( validator, rc );
	ref_put ( &validator->refcnt );
 err_alloc:
 err_sanity:
	return rc;
}
