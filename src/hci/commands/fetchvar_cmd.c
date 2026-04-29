/*
 * Copyright (C) 2026 Huzaifa Ali Zar <huzaifazar@gmail.com>.
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
FILE_SECBOOT ( PERMITTED );

#include <stdio.h>
#include <getopt.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <usr/fetchvar.h>

/** @file
 *
 * fetchvar command
 *
 */

/** "fetchvar" options */
struct fetchvar_options {};

/** "fetchvar" option list */
static struct option_descriptor fetchvar_opts[] = {};

/** "fetchvar" command descriptor */
static struct command_descriptor fetchvar_cmd =
	COMMAND_DESC ( struct fetchvar_options, fetchvar_opts, 2, 2,
		       "<setting> <uri>" );

/**
 * The "fetchvar" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int fetchvar_exec ( int argc, char **argv ) {
	struct fetchvar_options opts;
	const char *setting_name;
	const char *uri_string;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &fetchvar_cmd, &opts ) ) != 0 )
		return rc;

	/* Parse setting name */
	setting_name = argv[optind];

	/* Parse URI */
	uri_string = argv[ optind + 1 ];

	/* Fetch URI and store in setting */
	if ( ( rc = fetchvar ( uri_string, setting_name ) ) != 0 )
		return rc;

	return 0;
}

/** The "fetchvar" command */
COMMAND ( fetchvar, fetchvar_exec );
