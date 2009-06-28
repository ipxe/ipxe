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
#include <gpxe/netdevice.h>
#include <gpxe/command.h>
#include <usr/ifmgmt.h>
#include <hci/ifmgmt_cmd.h>

/** @file
 *
 * Network interface management commands
 *
 */

/** Options shared by all if<xxx> commands */
static struct option ifcommon_longopts[] = {
	{ "help", 0, NULL, 'h' },
	{ NULL, 0, NULL, 0 },
};

/**
 * Print syntax of if<xxx> command
 *
 * @v argv		Command arguments
 * @v verb		Verb describing the action of the command
 */
static void ifcommon_syntax ( char **argv, const char *verb ) {
	printf ( "Usage:\n"
		 "  %s [<interface>] [<interface>...]\n"
		 "\n"
		 "%s the specified network interfaces\n",
		 argv[0], verb );
}

/**
 * Execute if<xxx> command over all network devices
 *
 * @v payload		Command to execute
 * @ret rc		Exit code
 */
static int ifcommon_do_all ( int ( * payload ) ( struct net_device * ) ) {
	struct net_device *netdev;
	int rc = 0;

	/* Execute payload for each network device */
	for_each_netdev ( netdev ) {
		if ( payload ( netdev ) != 0 )
			rc = 1;
	}
	return rc;
}

/**
 * Execute if<xxx> command over list of network devices
 *
 * @v payload		Command to execute
 * @ret rc		Exit code
 */
static int ifcommon_do_list ( int ( * payload ) ( struct net_device * ),
			      char **list, unsigned int count ) {
	const char *netdev_name;
	struct net_device *netdev;
	int rc = 0;

	while ( count-- ) {
		netdev_name = *(list++);
		netdev = find_netdev ( netdev_name );
		if ( ! netdev ) {
			printf ( "%s: no such interface\n", netdev_name );
			rc = 1;
			continue;
		}
		if ( payload ( netdev ) != 0 )
			rc = 1;
	}
	return rc;
}

/**
 * Execute if<xxx> command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @v payload		Command to execute
 * @v verb		Verb describing the action of the command
 * @ret rc		Exit code
 */
int ifcommon_exec ( int argc, char **argv,
		    int ( * payload ) ( struct net_device * ),
		    const char *verb ) {
	int c;

	/* Parse options */
	while ( ( c = getopt_long ( argc, argv, "h", ifcommon_longopts,
				    NULL ) ) >= 0 ) {
		switch ( c ) {
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			ifcommon_syntax ( argv, verb );
			return 1;
		}
	}

	if ( optind == argc ) {
		return ifcommon_do_all ( payload );
	} else {
		return ifcommon_do_list ( payload, &argv[optind],
					  ( argc - optind ) );
	}
}

/* "ifopen" command */

static int ifopen_payload ( struct net_device *netdev ) {
	return ifopen ( netdev );
}

static int ifopen_exec ( int argc, char **argv ) {
	return ifcommon_exec ( argc, argv, ifopen_payload, "Open" );
}

/* "ifclose" command */

static int ifclose_payload ( struct net_device *netdev ) {
	ifclose ( netdev );
	return 0;
}

static int ifclose_exec ( int argc, char **argv ) {
	return ifcommon_exec ( argc, argv, ifclose_payload, "Close" );
}

/* "ifstat" command */

static int ifstat_payload ( struct net_device *netdev ) {
	ifstat ( netdev );
	return 0;
}

static int ifstat_exec ( int argc, char **argv ) {
	return ifcommon_exec ( argc, argv,
			       ifstat_payload, "Display status of" );
}

/** Interface management commands */
struct command ifmgmt_commands[] __command = {
	{
		.name = "ifopen",
		.exec = ifopen_exec,
	},
	{
		.name = "ifclose",
		.exec = ifclose_exec,
	},
	{
		.name = "ifstat",
		.exec = ifstat_exec,
	},
};
