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
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ipxe/netdevice.h>
#include <ipxe/command.h>
#include <ipxe/vlan.h>

/** @file
 *
 * VLAN commands
 *
 */

static void vcreate_syntax ( char **argv ) {
	printf ( "Usage:\n  %s --tag <tag> [--priority <priority] "
		 "<trunk interface>\n", argv[0] );
}

static int vcreate_exec ( int argc, char **argv ) {
	static struct option vcreate_opts[] = {
		{ "help", 0, NULL, 'h' },
		{ "tag", required_argument, NULL, 't' },
		{ "priority", required_argument, NULL, 'p' },
		{ NULL, 0, NULL, 0 },
	};
	const char *trunk_name;
	const char *tag_text = NULL;
	const char *priority_text = NULL;
	struct net_device *trunk;
	unsigned int tag;
	unsigned int priority;
	char *endp;
	int c;
	int rc;

	/* Parse command line */
	while ( ( c = getopt_long ( argc, argv, "ht:p:", vcreate_opts,
				    NULL ) ) >= 0 ) {
		switch ( c ) {
		case 't':
			tag_text = optarg;
			break;
		case 'p':
			priority_text = optarg;
			break;
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			vcreate_syntax ( argv );
			return 1;
		}
	}
	if ( optind != ( argc - 1 ) ) {
		vcreate_syntax ( argv );
		return 1;
	}
	trunk_name = argv[optind];
	if ( ! tag_text ) {
		vcreate_syntax ( argv );
		return 1;
	}

	/* Identify network device */
	trunk = find_netdev ( trunk_name );
	if ( ! trunk ) {
		printf ( "%s: no such interface\n", trunk_name );
		return 1;
	}
	tag = strtoul ( tag_text, &endp, 10 );
	if ( *endp ) {
		printf ( "%s: invalid tag\n", tag_text );
		return 1;
	}
	if ( priority_text ) {
		priority = strtoul ( priority_text, &endp, 10 );
		if ( *endp ) {
			printf ( "%s: invalid priority\n", priority_text );
			return 1;
		}
	} else {
		priority = 0;
	}

	/* Create VLAN device */
	if ( ( rc = vlan_create ( trunk, tag, priority ) ) != 0 ) {
		printf ( "Could not create VLAN device: %s\n",
			 strerror ( rc ) );
		return 1;
	}

	return 0;
}

static void vdestroy_syntax ( char **argv ) {
	printf ( "Usage:\n  %s <interface>\n", argv[0] );
}

static int vdestroy_exec ( int argc, char **argv ) {
	static struct option vdestroy_opts[] = {
		{ "help", 0, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};
	const char *netdev_name;
	struct net_device *netdev;
	int c;
	int rc;

	/* Parse command line */
	while ( ( c = getopt_long ( argc, argv, "h", vdestroy_opts,
				    NULL ) ) >= 0 ) {
		switch ( c ) {
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			vdestroy_syntax ( argv );
			return 1;
		}
	}
	if ( optind != ( argc - 1 ) ) {
		vdestroy_syntax ( argv );
		return 1;
	}
	netdev_name = argv[optind];

	/* Identify network device */
	netdev = find_netdev ( netdev_name );
	if ( ! netdev ) {
		printf ( "%s: no such interface\n", netdev_name );
		return 1;
	}

	/* Destroy VLAN device */
	if ( ( rc = vlan_destroy ( netdev ) ) != 0 ) {
		printf ( "Could not destroy VLAN device: %s\n",
			 strerror ( rc ) );
		return 1;
	}

	return 0;
}

struct command vlan_commands[] __command = {
	{
		.name = "vcreate",
		.exec = vcreate_exec,
	},
	{
		.name = "vdestroy",
		.exec = vdestroy_exec,
	},
};
