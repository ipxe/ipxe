/*
 * Copyright (C) 2010 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <ipxe/settings.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>

FILE_LICENCE ( GPL2_OR_LATER );

/** @file
 *
 * Non-volatile option commands
 *
 */

/** "show" options */
struct show_options {};

/** "show" option list */
static struct option_descriptor show_opts[] = {};

/** "show" command descriptor */
static struct command_descriptor show_cmd =
	COMMAND_DESC ( struct show_options, show_opts, 1, 1, "<setting>" );

/**
 * "show" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int show_exec ( int argc, char **argv ) {
	struct show_options opts;
	const char *name;
	char buf[256];
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &show_cmd, &opts ) ) != 0 )
		return rc;

	/* Parse setting name */
	name = argv[optind];

	/* Fetch setting */
	if ( ( rc = fetchf_named_setting ( name, buf,
					   sizeof ( buf ) ) ) < 0 ) {
		printf ( "Could not find \"%s\": %s\n",
			 name, strerror ( rc ) );
		return rc;
	}

	/* Print setting value */
	printf ( "%s = %s\n", name, buf );

	return 0;
}

/** "set" options */
struct set_options {};

/** "set" option list */
static struct option_descriptor set_opts[] = {};

/** "set" command descriptor */
static struct command_descriptor set_cmd =
	COMMAND_DESC ( struct set_options, set_opts, 1, MAX_ARGUMENTS,
		       "<setting> <value>" );

/**
 * "set" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int set_exec ( int argc, char **argv ) {
	struct set_options opts;
	const char *name;
	char *value;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &set_cmd, &opts ) ) != 0 )
		goto err_parse_options;

	/* Parse setting name */
	name = argv[optind];

	/* Parse setting value */
	value = concat_args ( &argv[ optind + 1 ] );
	if ( ! value ) {
		rc = -ENOMEM;
		goto err_concat_args;
	}

	/* Determine total length of command line */
	if ( ( rc = storef_named_setting ( name, value ) ) != 0 ) {
		printf ( "Could not set \"%s\"=\"%s\": %s\n",
			 name, value, strerror ( rc ) );
		goto err_store;
	}

	free ( value );
	return 0;

 err_store:
	free ( value );
 err_concat_args:
 err_parse_options:
	return rc;
}

/** "clear" options */
struct clear_options {};

/** "clear" option list */
static struct option_descriptor clear_opts[] = {};

/** "clear" command descriptor */
static struct command_descriptor clear_cmd =
	COMMAND_DESC ( struct clear_options, clear_opts, 1, 1, "<setting>" );

/**
 * "clear" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int clear_exec ( int argc, char **argv ) {
	struct clear_options opts;
	const char *name;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &clear_cmd, &opts ) ) != 0 )
		return rc;

	/* Parse setting name */
	name = argv[optind];

	/* Clear setting */
	if ( ( rc = delete_named_setting ( name ) ) != 0 ) {
		printf ( "Could not clear \"%s\": %s\n",
			 name, strerror ( rc ) );
		return rc;
	}
	
	return 0;
}

/** Non-volatile option commands */
struct command nvo_commands[] __command = {
	{
		.name = "show",
		.exec = show_exec,
	},
	{
		.name = "set",
		.exec = set_exec,
	},	
	{
		.name = "clear",
		.exec = clear_exec,
	},
};
