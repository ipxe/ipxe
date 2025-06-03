/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <usr/fdtmgmt.h>

/** @file
 *
 * Flattened Device Tree commands
 *
 */

/** "fdt" options */
struct fdt_options {
	/** Download timeout */
	unsigned long timeout;
};

/** "fdt" option list */
static struct option_descriptor fdt_opts[] = {
	OPTION_DESC ( "timeout", 't', required_argument,
		      struct fdt_options, timeout, parse_timeout ),
};

/** "fdt" command descriptor */
static struct command_descriptor fdt_cmd =
	COMMAND_DESC ( struct fdt_options, fdt_opts, 0, 1, "[<uri>]" );

/**
 * The "fdt" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int fdt_exec ( int argc, char **argv ) {
	struct fdt_options opts;
	struct image *image = NULL;
	char *name_uri;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &fdt_cmd, &opts ) ) != 0 )
		goto err_parse;

	/* Parse name/URI string */
	name_uri = argv[optind];

	/* Acquire image, if applicable */
	if ( name_uri && ( ( rc = imgacquire ( name_uri, opts.timeout,
					       &image ) ) != 0 ) ) {
		goto err_image;
	}

	/* (Un)register as FDT */
	if ( ( rc = imgfdt ( image ) ) != 0 )
		goto err_fdt;

 err_fdt:
 err_image:
 err_parse:
	return rc;
}

/** Flattened Device Tree commands */
struct command fdt_commands[] __command = {
	{
		.name = "fdt",
		.exec = fdt_exec,
	},
};
