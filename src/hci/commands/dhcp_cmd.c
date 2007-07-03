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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <gpxe/netdevice.h>
#include <gpxe/command.h>
#include <usr/dhcpmgmt.h>

/** @file
 *
 * DHCP management commands
 *
 */

/**
 * "dhcp" command syntax message
 *
 * @v argv		Argument list
 */
static void dhcp_syntax ( char **argv ) {
	printf ( "Usage:\n"
		 "  %s <interface>\n"
		 "\n"
		 "Configure a network interface using DHCP\n",
		 argv[0] );
}

/**
 * The "dhcp" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int dhcp_exec ( int argc, char **argv ) {
	static struct option longopts[] = {
		{ "help", 0, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};
	const char *name;
	struct net_device *netdev;
	int c;
	int rc;

	/* Parse options */
	while ( ( c = getopt_long ( argc, argv, "h", longopts, NULL ) ) >= 0 ){
		switch ( c ) {
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			dhcp_syntax ( argv );
			return 1;
		}
	}

	/* Need exactly one interface name remaining after the options */
	if ( optind != ( argc - 1 ) ) {
		dhcp_syntax ( argv );
		return 1;
	}
	name = argv[optind];

	/* Perform DHCP */
	netdev = find_netdev ( name );
	if ( ! netdev ) {
		printf ( "No such interface: %s\n", name );
		return 1;
	}
	if ( ( rc = dhcp ( netdev ) ) != 0 ) {
		printf ( "Could not configure %s: %s\n", netdev->name,
			 strerror ( rc ) );
		return 1;
	}

	return 0;
}

/** DHCP management commands */
struct command dhcp_commands[] __command = {
	{
		.name = "dhcp",
		.exec = dhcp_exec,
	},
};
