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
#include <ipxe/if_ether.h>
#include <usr/lotest.h>

/** @file
 *
 * Loopback testing commands
 *
 */

static void lotest_syntax ( char **argv ) {
	printf ( "Usage:\n  %s <sending interface> <receiving interface>\n",
		 argv[0] );
}

static int lotest_exec ( int argc, char **argv ) {
	static struct option lotest_opts[] = {
		{ "help", 0, NULL, 'h' },
		{ "mtu", required_argument, NULL, 'm' },
		{ NULL, 0, NULL, 0 },
	};
	const char *sender_name;
	const char *receiver_name;
	const char *mtu_text = NULL;
	struct net_device *sender;
	struct net_device *receiver;
	char *endp;
	size_t mtu;
	int c;
	int rc;

	/* Parse command line */
	while ( ( c = getopt_long ( argc, argv, "hm:", lotest_opts,
				    NULL ) ) >= 0 ) {
		switch ( c ) {
		case 'm':
			mtu_text = optarg;
			break;
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			lotest_syntax ( argv );
			return 1;
		}
	}
	if ( optind != ( argc - 2 ) ) {
		lotest_syntax ( argv );
		return 1;
	}
	sender_name = argv[optind];
	receiver_name = argv[optind + 1];

	/* Identify network devices */
	sender = find_netdev ( sender_name );
	if ( ! sender ) {
		printf ( "%s: no such interface\n", sender_name );
		return 1;
	}
	receiver = find_netdev ( receiver_name );
	if ( ! receiver ) {
		printf ( "%s: no such interface\n", receiver_name );
		return 1;
	}

	/* Identify MTU */
	if ( mtu_text ) {
		mtu = strtoul ( mtu_text, &endp, 10 );
		if ( *endp ) {
			printf ( "%s: invalid MTU\n", mtu_text );
			return 1;
		}
	} else {
		mtu = ETH_MAX_MTU;
	}

	/* Perform loopback test */
	if ( ( rc = loopback_test ( sender, receiver, mtu ) ) != 0 ) {
		printf ( "Test failed: %s\n", strerror ( rc ) );
		return 1;
	}

	return 0;
}

struct command lotest_command __command = {
	.name = "lotest",
	.exec = lotest_exec,
};
