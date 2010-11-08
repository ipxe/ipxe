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

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdio.h>
#include <getopt.h>
#include <strings.h>
#include <ipxe/fc.h>
#include <ipxe/fcels.h>
#include <ipxe/command.h>
#include <ipxe/tables.h>
#include <usr/fcmgmt.h>

/** @file
 *
 * Fibre Channel management commands
 *
 */

static void fcstat_syntax ( char **argv ) {
	printf ( "Usage:\n  %s\n", argv[0] );
}

static int fcstat_exec ( int argc, char **argv ) {
	static struct option fcstat_opts[] = {
		{ "help", 0, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};
	struct fc_port *port;
	struct fc_peer *peer;
	int c;

	/* Parse options */
	while ( ( c = getopt_long ( argc, argv, "h", fcstat_opts,
				    NULL ) ) >= 0 ) {
		switch ( c ) {
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			fcstat_syntax ( argv );
			return 1;
		}
	}

	if ( optind != argc ) {
		fcstat_syntax ( argv );
		return 1;
	}

	list_for_each_entry ( port, &fc_ports, list )
		fcportstat ( port );
	list_for_each_entry ( peer, &fc_peers, list )
		fcpeerstat ( peer );

	return 0;
}

static void fcels_syntax ( char **argv ) {
	printf ( "Usage:\n  %s [--port <port>] [--id <peer port id>]"
		 " <command>\n", argv[0] );
}

static struct fc_els_handler * fcels_find_handler ( const char *name ) {
	struct fc_els_handler *handler;

	for_each_table_entry ( handler, FC_ELS_HANDLERS ) {
		if ( strcasecmp ( handler->name, name ) == 0 )
			return handler;
	}
	return NULL;
}

static int fcels_exec ( int argc, char **argv ) {
	static struct option fcels_opts[] = {
		{ "help", 0, NULL, 'h' },
		{ "port", required_argument, NULL, 'p' },
		{ "id", required_argument, NULL, 'i' },
		{ NULL, 0, NULL, 0 },
	};
	const char *handler_text;
	const char *port_text = NULL;
	const char *id_text = NULL;
	struct fc_els_handler *handler;
	struct fc_port *port;
	struct fc_port_id id_buf;
	struct fc_port_id *id;
	int c;

	/* Parse options */
	while ( ( c = getopt_long ( argc, argv, "hp:i:", fcels_opts,
				    NULL ) ) >= 0 ) {
		switch ( c ) {
		case 'p':
			port_text = optarg;
			break;
		case 'i':
			id_text = optarg;
			break;
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			fcels_syntax ( argv );
			return 1;
		}
	}

	/* Identify ELS */
	if ( optind != ( argc - 1 ) ) {
		fcels_syntax ( argv );
		return 1;
	}
	handler_text = argv[optind];
	handler = fcels_find_handler ( handler_text );
	if ( ! handler ) {
		printf ( "%s: unrecognised ELS\n", handler_text );
		return 1;
	}

	/* Identify port */
	if ( port_text ) {
		/* Use specified port */
		port = fc_port_find ( port_text );
		if ( ! port ) {
			printf ( "%s: no such port\n", port_text );
			return 1;
		}
	} else {
		/* Use first port */
		port = list_first_entry ( &fc_ports, struct fc_port, list );
		if ( ! port ) {
			printf ( "No ports\n" );
			return 1;
		}
	}
	assert ( port != NULL );

	/* Identify port ID */
	if ( id_text ) {
		if ( fc_id_aton ( id_text, &id_buf ) != 0 ) {
			printf ( "%s: invalid port ID\n", id_text );
			return 1;
		}
		id = &id_buf;
	} else {
		if ( fc_link_ok ( &port->link ) &&
		     ! ( port->flags & FC_PORT_HAS_FABRIC ) ) {
			id = &port->ptp_link_port_id;
		} else {
			id = &fc_f_port_id;
		}
	}
	assert ( id != NULL );

	if ( fcels ( port, id, handler ) != 0 )
		return 1;

	return 0;
}

/** Fibre Channel management commands */
struct command fcmgmt_commands[] __command = {
	{
		.name = "fcstat",
		.exec = fcstat_exec,
	},
	{
		.name = "fcels",
		.exec = fcels_exec,
	},
};
