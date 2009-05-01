/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdio.h>
#include <getopt.h>
#include <gpxe/command.h>
#include <usr/route.h>

/** @file
 *
 * Routing table management commands
 *
 */

/**
 * "route" command syntax message
 *
 * @v argv		Argument list
 */
static void route_syntax ( char **argv ) {
	printf ( "Usage:\n"
		 "  %s\n"
		 "\n"
		 "Displays the routing table\n",
		 argv[0] );
}

/**
 * The "route" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int route_exec ( int argc, char **argv ) {
	static struct option longopts[] = {
		{ "help", 0, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};

	int c;

	/* Parse options */
	while ( ( c = getopt_long ( argc, argv, "h", longopts, NULL ) ) >= 0 ){
		switch ( c ) {
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			route_syntax ( argv );
			return 1;
		}
	}

	if ( optind != argc ) {
		route_syntax ( argv );
		return 1;
	}

	route();
	return 0;
}

/** Routing table management commands */
struct command route_commands[] __command = {
	{
		.name = "route",
		.exec = route_exec,
	},
};
