/*
 * Copyright (C) 2008 Stefan Hajnoczi <stefanha@gmail.com>.
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

#include <stdio.h>
#include <getopt.h>
#include <gpxe/command.h>
#include <gpxe/gdbstub.h>

/** @file
 *
 * GDB stub command
 *
 */

/**
 * "gdbstub" command syntax message
 *
 * @v argv		Argument list
 */
static void gdbstub_syntax ( char **argv ) {
	printf ( "Usage:\n"
		 "  %s <transport> [<options>...]\n"
		 "\n"
		 "Start remote debugging using one of the following transports:\n"
		 "  serial           use serial port (if compiled in)\n"
		 "  udp <interface>  use UDP over network interface (if compiled in)\n",
		 argv[0] );
}

/**
 * The "gdbstub" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int gdbstub_exec ( int argc, char **argv ) {
	static struct option longopts[] = {
		{ "help", 0, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};
	const char *trans_name;
	struct gdb_transport *trans;
	int c;

	/* Parse options */
	while ( ( c = getopt_long ( argc, argv, "h", longopts, NULL ) ) >= 0 ){
		switch ( c ) {
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			gdbstub_syntax ( argv );
			return 1;
		}
	}

	/* At least one argument */
	if ( optind == argc ) {
		gdbstub_syntax ( argv );
		return 1;
	}

	trans_name = argv[optind++];

	/* Initialise transport */
	trans = find_gdb_transport ( trans_name );
	if ( !trans ) {
		printf ( "%s: no such transport (is it compiled in?)\n", trans_name );
		return 1;
	}

	if ( trans->init ) {
		if ( trans->init ( argc - optind, &argv[optind] ) != 0 ) {
			return 1;
		}
	}

	/* Enter GDB stub */
	gdbstub_start ( trans );
	return 0;
}

/** GDB stub commands */
struct command gdbstub_commands[] __command = {
	{
		.name = "gdbstub",
		.exec = gdbstub_exec,
	},
};
