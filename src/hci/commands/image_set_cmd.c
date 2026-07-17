/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
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
FILE_SECBOOT ( PERMITTED );

#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <ipxe/settings.h>
#include <usr/imgmgmt.h>

/** @file
 *
 * Image setting commands
 *
 */

/** "imgset" options */
struct imgset_options {
	/** Keep original image */
	int keep;
	/** Download timeout */
	unsigned long timeout;
	/** Display only error messages */
	int quiet;
};

/** "imgset" option list */
static struct option_descriptor imgset_opts[] = {
	OPTION_DESC ( "keep", 'k', no_argument,
		      struct imgset_options, keep, parse_flag ),
	OPTION_DESC ( "timeout", 't', required_argument,
		      struct imgset_options, timeout, parse_timeout ),
	OPTION_DESC ( "quiet", 'q', no_argument,
		      struct imgset_options, quiet, parse_flag ),
};

/** "imgset" command descriptor */
static struct command_descriptor imgset_cmd =
	COMMAND_DESC ( struct imgset_options, imgset_opts, 2, 2, NULL );

/**
 * The "imgset" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int imgset_exec ( int argc, char **argv ) {
	struct imgset_options opts;
	struct named_setting setting;
	struct image *image;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &imgset_cmd, &opts ) ) != 0 )
		goto err_parse;

	/* Parse setting name */
	if ( ( rc = parse_autovivified_setting ( argv[optind++],
						 &setting ) ) != 0 )
		goto err_setting;

	/* Acquire image */
	if ( ( rc = imgacquire ( argv[optind], opts.timeout, opts.quiet,
				 &image ) ) != 0 )
		goto err_acquire;

	/* Store setting value */
	if ( ( rc = store_setting ( setting.settings, &setting.setting,
				    image->data, image->len ) ) != 0 ) {
		printf ( "Could not store \"%s\": %s\n",
			 setting.setting.name, strerror ( rc ) );
		goto err_store;
	}

	/* Success */
	rc = 0;

 err_store:
	/* Discard image unless --keep was specified */
	if ( ! opts.keep )
		unregister_image ( image );
 err_acquire:
 err_setting:
 err_parse:
	return rc;
}

/* Image setting commands */
COMMAND ( imgset, imgset_exec );
