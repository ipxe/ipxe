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
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <gpxe/netdevice.h>
#include <gpxe/in.h>
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
	const char *netdev_txt;
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
	netdev_txt = argv[optind];

	/* Parse arguments */
	netdev = find_netdev ( netdev_txt );
	if ( ! netdev ) {
		printf ( "No such interface: %s\n", netdev_txt );
		return 1;
	}

	/* Perform DHCP */
	if ( ( rc = dhcp ( netdev ) ) != 0 ) {
		printf ( "Could not configure %s: %s\n", netdev->name,
			 strerror ( rc ) );
		return 1;
	}

	return 0;
}

/**
 * "pxebs" command syntax message
 *
 * @v argv		Argument list
 */
static void pxebs_syntax ( char **argv ) {
	printf ( "Usage:\n"
		 "  %s <interface> <server_type>\n"
		 "\n"
		 "Perform PXE Boot Server discovery\n",
		 argv[0] );
}

/**
 * The "pxebs" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int pxebs_exec ( int argc, char **argv ) {
	static struct option longopts[] = {
		{ "help", 0, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};
	const char *netdev_txt;
	const char *pxe_type_txt;
	struct net_device *netdev;
	unsigned int pxe_type;
	char *end;
	int c;
	int rc;

	/* Parse options */
	while ( ( c = getopt_long ( argc, argv, "h", longopts, NULL ) ) >= 0 ){
		switch ( c ) {
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			pxebs_syntax ( argv );
			return 1;
		}
	}
	if ( optind != ( argc - 2 ) ) {
		pxebs_syntax ( argv );
		return 1;
	}
	netdev_txt = argv[optind];
	pxe_type_txt = argv[ optind + 1 ];

	/* Parse arguments */
	netdev = find_netdev ( netdev_txt );
	if ( ! netdev ) {
		printf ( "No such interface: %s\n", netdev_txt );
		return 1;
	}
	pxe_type = strtoul ( pxe_type_txt, &end, 0 );
	if ( *end ) {
		printf ( "Bad server type: %s\n", pxe_type_txt );
		return 1;
	}

	/* Perform Boot Server Discovery */
	if ( ( rc = pxebs ( netdev, pxe_type ) ) != 0 ) {
		printf ( "Could not discover boot server on %s: %s\n",
			 netdev->name, strerror ( rc ) );
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
	{
		.name = "pxebs",
		.exec = pxebs_exec,
	},
};
