/*
 * Copyright (C) 2016 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <ipxe/x509.h>
#include <ipxe/certstore.h>
#include <ipxe/image.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <usr/imgmgmt.h>
#include <usr/certmgmt.h>

/** @file
 *
 * Certificate management commands
 *
 */

/** "cert<xxx>" options */
struct cert_options {
	/** Certificate subject name */
	char *name;
	/** Keep certificate file after parsing */
	int keep;
};

/** "cert<xxx>" option list */
static union {
	/* "certstore" takes both options */
	struct option_descriptor certstore[2];
	/* "certstat" takes only --subject */
	struct option_descriptor certstat[1];
	/* "certfree" takes only --subject */
	struct option_descriptor certfree[1];
} opts = {
	.certstore = {
		OPTION_DESC ( "subject", 's', required_argument,
			      struct cert_options, name, parse_string ),
		OPTION_DESC ( "keep", 'k', no_argument,
			      struct cert_options, keep, parse_flag ),
	},
};

/** A "cert<xxx>" command descriptor */
struct cert_command_descriptor {
	/** Command descriptor */
	struct command_descriptor cmd;
	/** Payload
	 *
	 * @v cert		X.509 certificate
	 * @ret rc		Return status code
	 */
	int ( * payload ) ( struct x509_certificate *cert );
};

/**
 * Construct "cert<xxx>" command descriptor
 *
 * @v _struct		Options structure type
 * @v _options		Option descriptor array
 * @v _min_args		Minimum number of non-option arguments
 * @v _max_args		Maximum number of non-option arguments
 * @v _usage		Command usage
 * @v _payload		Payload method
 * @ret _command	Command descriptor
 */
#define CERT_COMMAND_DESC( _struct, _options, _min_args, _max_args,	\
			   _usage, _payload )				\
	{								\
		.cmd = COMMAND_DESC ( _struct, _options, _min_args,	\
				      _max_args, _usage ),		\
		.payload = _payload,					\
	}

/**
 * Execute "cert<xxx>" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @v certcmd		Command descriptor
 * @ret rc		Return status code
 */
static int cert_exec ( int argc, char **argv,
		       struct cert_command_descriptor *certcmd ) {
	struct command_descriptor *cmd = &certcmd->cmd;
	struct cert_options opts;
	struct image *image = NULL;
	struct x509_certificate *cert;
	struct x509_certificate *tmp;
	unsigned int count = 0;
	size_t offset = 0;
	int next;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, cmd, &opts ) ) != 0 )
		goto err_parse;

	/* Acquire image, if applicable */
	if ( ( optind < argc ) &&
	     ( ( rc = imgacquire ( argv[optind], 0, &image ) ) != 0 ) )
		goto err_acquire;

	/* Get first entry in certificate store */
	tmp = list_first_entry ( &certstore.links, struct x509_certificate,
				 store.list );

	/* Iterate over certificates */
	while ( 1 ) {

		/* Get next certificate from image or store as applicable */
		if ( image ) {

			/* Get next certificate from image */
			if ( offset >= image->len )
				break;
			next = image_x509 ( image, offset, &cert );
			if ( next < 0 ) {
				rc = next;
				printf ( "Could not parse certificate: %s\n",
					 strerror ( rc ) );
				goto err_x509;
			}
			offset = next;

		} else {

			/* Get next certificate from store */
			cert = tmp;
			if ( ! cert )
				break;
			tmp = list_next_entry ( tmp, &certstore.links,
						store.list );
			x509_get ( cert );
		}

		/* Skip non-matching names, if a name was specified */
		if ( opts.name && ( x509_check_name ( cert, opts.name ) != 0 )){
			x509_put ( cert );
			continue;
		}

		/* Execute payload */
		if ( ( rc = certcmd->payload ( cert ) ) != 0 ) {
			x509_put ( cert );
			goto err_payload;
		}

		/* Count number of certificates processed */
		count++;

		/* Drop reference to certificate */
		x509_put ( cert );
	}

	/* Fail if a name was specified and no matching certificates
	 * were found.
	 */
	if ( opts.name && ( count == 0 ) ) {
		printf ( "\"%s\" : no such certificate\n", opts.name );
		rc = -ENOENT;
		goto err_none;
	}

 err_none:
 err_payload:
 err_x509:
	if ( image && ( ! opts.keep ) )
		unregister_image ( image );
 err_acquire:
 err_parse:
	return rc;
}

/**
 * "certstat" payload
 *
 * @v cert		X.509 certificate
 * @ret rc		Return status code
 */
static int certstat_payload ( struct x509_certificate *cert ) {

	certstat ( cert );
	return 0;
}

/** "certstat" command descriptor */
static struct cert_command_descriptor certstat_cmd =
	CERT_COMMAND_DESC ( struct cert_options, opts.certstat, 0, 0, NULL,
			    certstat_payload );

/**
 * The "certstat" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int certstat_exec ( int argc, char **argv ) {

	return cert_exec ( argc, argv, &certstat_cmd );
}

/**
 * "certstore" payload
 *
 * @v cert		X.509 certificate
 * @ret rc		Return status code
 */
static int certstore_payload ( struct x509_certificate *cert ) {

	/* Mark certificate as having been added explicitly */
	cert->flags |= X509_FL_EXPLICIT;

	return 0;
}

/** "certstore" command descriptor */
static struct cert_command_descriptor certstore_cmd =
	CERT_COMMAND_DESC ( struct cert_options, opts.certstore, 0, 1,
			    "[<uri|image>]", certstore_payload );

/**
 * The "certstore" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int certstore_exec ( int argc, char **argv ) {

	return cert_exec ( argc, argv, &certstore_cmd );
}

/**
 * "certfree" payload
 *
 * @v cert		X.509 certificate
 * @ret rc		Return status code
 */
static int certfree_payload ( struct x509_certificate *cert ) {

	/* Remove from certificate store */
	certstore_del ( cert );

	return 0;
}

/** "certfree" command descriptor */
static struct cert_command_descriptor certfree_cmd =
	CERT_COMMAND_DESC ( struct cert_options, opts.certfree, 0, 0, NULL,
			    certfree_payload );

/**
 * The "certfree" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int certfree_exec ( int argc, char **argv ) {

	return cert_exec ( argc, argv, &certfree_cmd );
}

/** Certificate management commands */
struct command certmgmt_commands[] __command = {
	{
		.name = "certstat",
		.exec = certstat_exec,
	},
	{
		.name = "certstore",
		.exec = certstore_exec,
	},
	{
		.name = "certfree",
		.exec = certfree_exec,
	},
};
