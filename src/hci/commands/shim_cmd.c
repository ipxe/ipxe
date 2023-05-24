/*
 * Copyright (C) 2023 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <ipxe/efi/efi_image.h>
#include <usr/imgmgmt.h>
#include <usr/shimmgmt.h>

/** @file
 *
 * EFI shim command
 *
 */

/* Exist as a dummy command on non-EFI platforms */
#ifdef PLATFORM_efi
#define shim_dummy 0
#else
#define shim_dummy 1
#endif

/** "shim" options */
struct shim_options {
	/** Download timeout */
	unsigned long timeout;
	/** Require third party loader */
	int require_loader;
	/** Allow PXE base code protocol */
	int allow_pxe;
	/** Allow SBAT variable access */
	int allow_sbat;
};

/** "shim" option list */
static struct option_descriptor shim_opts[] = {
	OPTION_DESC ( "timeout", 't', required_argument,
		      struct shim_options, timeout, parse_timeout ),
	OPTION_DESC ( "require-loader", 'l', no_argument,
		      struct shim_options, require_loader, parse_flag ),
	OPTION_DESC ( "allow-pxe", 'p', no_argument,
		      struct shim_options, allow_pxe, parse_flag ),
	OPTION_DESC ( "allow-sbat", 's', no_argument,
		      struct shim_options, allow_sbat, parse_flag ),
};

/** "shim" command descriptor */
static struct command_descriptor shim_cmd =
	COMMAND_DESC ( struct shim_options, shim_opts, 0, 1, NULL );

/**
 * The "shim" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int shim_exec ( int argc, char **argv ) {
	struct shim_options opts;
	struct image *image = NULL;
	struct image *kernel;
	char *name_uri;
	int download;
	int rc;

	/* Do absolutely nothing if this is a non-EFI platform */
	if ( shim_dummy ) {
		rc = 0;
		goto err_dummy;
	}

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &shim_cmd, &opts ) ) != 0 )
		goto err_parse;

	/* Decide whether or not to download images */
	kernel = find_image_tag ( &selected_image );
	download = ( ! ( kernel && efi_can_load ( kernel ) ) );

	/* Parse name/URI string */
	name_uri = argv[optind];

	/* Acquire image, if applicable */
	if ( download && name_uri &&
	     ( ( rc = imgacquire ( name_uri, opts.timeout,
				   &image ) ) != 0 ) ) {
		goto err_image;
	}

	/* (Un)register as shim */
	if ( ( rc = shim ( image, opts.require_loader, opts.allow_pxe,
			   opts.allow_sbat ) ) != 0 )
		goto err_shim;

 err_shim:
 err_image:
 err_parse:
 err_dummy:
	return rc;
}

/** Shim commands */
struct command shim_commands[] __command = {
	{
		.name = "shim",
		.exec = shim_exec,
	},
};
