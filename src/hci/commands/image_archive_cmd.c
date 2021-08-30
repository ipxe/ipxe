/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <getopt.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <usr/imgmgmt.h>
#include <usr/imgarchive.h>

/** @file
 *
 * Archive image commands
 *
 */

/** "imgextract" options */
struct imgextract_options {
	/** Image name */
	char *name;
	/** Keep original image */
	int keep;
	/** Download timeout */
	unsigned long timeout;
};

/** "imgextract" option list */
static struct option_descriptor imgextract_opts[] = {
	OPTION_DESC ( "name", 'n', required_argument,
		      struct imgextract_options, name, parse_string ),
	OPTION_DESC ( "keep", 'k', no_argument,
		      struct imgextract_options, keep, parse_flag ),
	OPTION_DESC ( "timeout", 't', required_argument,
		      struct imgextract_options, timeout, parse_timeout ),
};

/** "imgextract" command descriptor */
static struct command_descriptor imgextract_cmd =
	COMMAND_DESC ( struct imgextract_options, imgextract_opts, 1, 1, NULL );

/**
 * The "imgextract" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int imgextract_exec ( int argc, char **argv ) {
	struct imgextract_options opts;
	struct image *image;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &imgextract_cmd,
				    &opts ) ) != 0 )
		goto err_parse;

	/* Acquire image */
	if ( ( rc = imgacquire ( argv[optind], opts.timeout, &image ) ) != 0 )
		goto err_acquire;

	/* Extract archive image */
	if ( ( rc = imgextract ( image, opts.name ) ) != 0 )
		goto err_extract;

	/* Success */
	rc = 0;

 err_extract:
	/* Discard original image unless --keep was specified */
	if ( ! opts.keep )
		unregister_image ( image );
 err_acquire:
 err_parse:
	return rc;
}

/** Archive image commands */
struct command image_archive_commands[] __command = {
	{
		.name = "imgextract",
		.exec = imgextract_exec,
	},
};
