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
#include <strings.h>
#include <errno.h>
#include <getopt.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <ipxe/http.h>
#include <usr/fetchvar.h>

/** @file
 *
 * fetchvar command
 *
 */

/** "fetchvar" options */
struct fetchvar_options {
	/** HTTP method name */
	char *method;
};

/** "fetchvar" option list */
static struct option_descriptor fetchvar_opts[] = {
	OPTION_DESC ( "method", 'm', required_argument,
		      struct fetchvar_options, method, parse_string ),
};

/** "fetchvar" command descriptor */
static struct command_descriptor fetchvar_cmd =
	COMMAND_DESC ( struct fetchvar_options, fetchvar_opts, 2, 2,
		       "[--method <method>] <setting> <uri>" );

/**
 * Parse HTTP method name
 *
 * @v name		Method name (e.g. "GET", "PUT")
 * @ret method		HTTP method, or NULL if not recognised
 */
static struct http_method * fetchvar_method ( const char *name ) {

	if ( strcasecmp ( name, "GET" ) == 0 )
		return &http_get;
	if ( strcasecmp ( name, "POST" ) == 0 )
		return &http_post;
	if ( strcasecmp ( name, "PUT" ) == 0 )
		return &http_put;
	if ( strcasecmp ( name, "HEAD" ) == 0 )
		return &http_head;
	return NULL;
}

/**
 * The "fetchvar" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int fetchvar_exec ( int argc, char **argv ) {
	struct fetchvar_options opts;
	struct http_method *method = NULL;
	const char *setting_name;
	const char *uri_string;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &fetchvar_cmd, &opts ) ) != 0 )
		return rc;

	/* Parse method (if specified) */
	if ( opts.method ) {
		method = fetchvar_method ( opts.method );
		if ( ! method ) {
			printf ( "Unsupported HTTP method \"%s\"\n",
				 opts.method );
			return -ENOTSUP;
		}
	}

	/* Parse setting name */
	setting_name = argv[optind];

	/* Parse URI */
	uri_string = argv[ optind + 1 ];

	/* Fetch URI and store in setting */
	if ( ( rc = fetchvar ( uri_string, setting_name, method ) ) != 0 )
		return rc;

	return 0;
}

/** The "fetchvar" command */
COMMAND ( fetchvar, fetchvar_exec );
