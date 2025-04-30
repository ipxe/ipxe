/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <ipxe/image.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <usr/imgmgmt.h>
#include <usr/imgcrypt.h>

/** @file
 *
 * Image encryption management commands
 *
 */

/** "imgdecrypt" options */
struct imgdecrypt_options {
	/** Decrypted image name */
	char *name;
	/** Keep envelope after decryption */
	int keep;
	/** Download timeout */
	unsigned long timeout;
};

/** "imgdecrypt" option list */
static struct option_descriptor imgdecrypt_opts[] = {
	OPTION_DESC ( "name", 'n', required_argument,
		      struct imgdecrypt_options, name, parse_string ),
	OPTION_DESC ( "keep", 'k', no_argument,
		      struct imgdecrypt_options, keep, parse_flag ),
	OPTION_DESC ( "timeout", 't', required_argument,
		      struct imgdecrypt_options, timeout, parse_timeout),
};

/** "imgdecrypt" command descriptor */
static struct command_descriptor imgdecrypt_cmd =
	COMMAND_DESC ( struct imgdecrypt_options, imgdecrypt_opts, 2, 2,
		       "<uri|image> <envelope uri|image>" );

/**
 * The "imgdecrypt" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int imgdecrypt_exec ( int argc, char **argv ) {
	struct imgdecrypt_options opts;
	const char *image_name_uri;
	const char *envelope_name_uri;
	struct image *image;
	struct image *envelope;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &imgdecrypt_cmd, &opts ) ) != 0)
		return rc;

	/* Parse image name/URI string */
	image_name_uri = argv[optind];

	/* Parse envelope name/URI string */
	envelope_name_uri = argv[ optind + 1 ];

	/* Acquire the image */
	if ( ( rc = imgacquire ( image_name_uri, opts.timeout, &image ) ) != 0 )
		goto err_acquire_image;

	/* Acquire the envelope image */
	if ( ( rc = imgacquire ( envelope_name_uri, opts.timeout,
				 &envelope ) ) != 0 )
		goto err_acquire_envelope;

	/* Decrypt image */
	if ( ( rc = imgdecrypt ( image, envelope, opts.name ) ) != 0 ) {
		printf ( "Could not decrypt: %s\n", strerror ( rc ) );
		goto err_decrypt;
	}

	/* Success */
	rc = 0;

 err_decrypt:
	/* Discard envelope unless --keep was specified */
	if ( ! opts.keep )
		unregister_image ( envelope );
 err_acquire_envelope:
 err_acquire_image:
	return rc;
}

/** Image encryption management commands */
struct command image_crypt_commands[] __command = {
	{
		.name = "imgdecrypt",
		.exec = imgdecrypt_exec,
	},
};
