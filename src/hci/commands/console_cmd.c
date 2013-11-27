/*
 * Copyright (C) 2013 Michael Brown <mbrown@fensystems.co.uk>.
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
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** @file
 *
 * Console management commands
 *
 */

#include <string.h>
#include <getopt.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <ipxe/console.h>
#include <ipxe/image.h>
#include <ipxe/pixbuf.h>
#include <usr/imgmgmt.h>

/** "console" options */
struct console_options {
	/** Console configuration */
	struct console_configuration config;
	/** Picture URI */
	char *picture;
	/** Keep picture after configuration */
	int keep;
};

/** "console" option list */
static struct option_descriptor console_opts[] = {
	OPTION_DESC ( "x", 'x', required_argument,
		      struct console_options, config.width, parse_integer ),
	OPTION_DESC ( "y", 'y', required_argument,
		      struct console_options, config.height, parse_integer ),
	OPTION_DESC ( "bpp", 'b', required_argument,
		      struct console_options, config.bpp, parse_integer ),
	OPTION_DESC ( "picture", 'p', required_argument,
		      struct console_options, picture, parse_string ),
	OPTION_DESC ( "keep", 'k', no_argument,
		      struct console_options, keep, parse_flag ),
};

/** "console" command descriptor */
static struct command_descriptor console_cmd =
	COMMAND_DESC ( struct console_options, console_opts, 0, 0, NULL );

/**
 * "console" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int console_exec ( int argc, char **argv ) {
	struct console_options opts;
	struct image *image = NULL;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &console_cmd, &opts ) ) != 0 )
		goto err_parse;

	/* Handle background picture, if applicable */
	if ( opts.picture ) {

		/* Acquire image */
		if ( ( rc = imgacquire ( opts.picture, &image ) ) != 0 )
			goto err_acquire;

		/* Convert to pixel buffer */
		if ( ( rc = image_pixbuf ( image, &opts.config.pixbuf ) ) != 0){
			printf ( "Could not use picture: %s\n",
				 strerror ( rc ) );
			goto err_pixbuf;
		}

		/* Apply image's width and height if none specified */
		if ( ! opts.config.width )
			opts.config.width = opts.config.pixbuf->width;
		if ( ! opts.config.height )
			opts.config.height = opts.config.pixbuf->height;
	}

	/* Configure console */
	if ( ( rc = console_configure ( &opts.config ) ) != 0 ) {
		printf ( "Could not configure console: %s\n", strerror ( rc ) );
		goto err_configure;
	}

 err_configure:
	pixbuf_put ( opts.config.pixbuf );
 err_pixbuf:
	/* Discard image unless --keep was specified */
	if ( image && ( ! opts.keep ) )
		unregister_image ( image );
 err_acquire:
 err_parse:
	return rc;
}

/** Console management commands */
struct command console_commands[] __command = {
	{
		.name = "console",
		.exec = console_exec,
	},
};
