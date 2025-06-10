/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Network device tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <string.h>
#include <stdio.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/test.h>
#include "netdev_test.h"

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int testnet_open ( struct net_device *netdev __unused ) {

	/* Do nothing, successfully */
	return 0;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void testnet_close ( struct net_device *netdev __unused ) {

	/* Do nothing */
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int testnet_transmit ( struct net_device *netdev,
			      struct io_buffer *iobuf ) {

	/* Complete immediately */
	netdev_tx_complete ( netdev, iobuf );
	return 0;
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void testnet_poll ( struct net_device *netdev __unused ) {

	/* Do nothing */
}

/** Test network device operations */
static struct net_device_operations testnet_operations = {
	.open		= testnet_open,
	.close		= testnet_close,
	.transmit	= testnet_transmit,
	.poll		= testnet_poll,
};

/**
 * Report a network device creation test result
 *
 * @v testnet		Test network device
 * @v file		Test code file
 * @v line		Test code line
 */
void testnet_okx ( struct testnet *testnet, const char *file,
		   unsigned int line ) {
	struct testnet_setting *testset;
	unsigned int i;

	/* Allocate device */
	testnet->netdev = alloc_etherdev ( 0 );
	okx ( testnet->netdev != NULL, file, line );
	netdev_init ( testnet->netdev, &testnet_operations );
	testnet->netdev->dev = &testnet->dev;
	snprintf ( testnet->netdev->name, sizeof ( testnet->netdev->name ),
		   "%s", testnet->dev.name );

	/* Register device */
	okx ( register_netdev ( testnet->netdev ) == 0, file, line );

	/* Open device */
	testnet_open_okx ( testnet, file, line );

	/* Apply initial settings */
	for ( i = 0 ; i < testnet->count ; i++ ) {
		testset = &testnet->testset[i];
		testnet_set_okx ( testnet, testset->name, testset->value,
				  file, line );
	}
}

/**
 * Report a network device opening test result
 *
 * @v testnet		Test network device
 * @v file		Test code file
 * @v line		Test code line
 */
void testnet_open_okx ( struct testnet *testnet, const char *file,
			unsigned int line ) {

	/* Sanity check */
	okx ( testnet->netdev != NULL, file, line );

	/* Open device */
	okx ( netdev_open ( testnet->netdev ) == 0, file, line );
}

/**
 * Report a network device setting test result
 *
 * @v testnet		Test network device
 * @v name		Setting name (relative to network device's settings)
 * @v value		Setting value
 * @v file		Test code file
 * @v line		Test code line
 */
void testnet_set_okx ( struct testnet *testnet, const char *name,
		       const char *value, const char *file,
		       unsigned int line ) {
	char fullname[ strlen ( testnet->dev.name ) + 1 /* "." or "/" */ +
		       strlen ( name ) + 1 /* NUL */ ];
	struct settings *settings;
	struct setting setting;

	/* Sanity check */
	okx ( testnet->netdev != NULL, file, line );
	settings = netdev_settings ( testnet->netdev );
	okx ( settings != NULL, file, line );
	okx ( strcmp ( settings->name, testnet->dev.name ) == 0, file, line );

	/* Construct setting name */
	snprintf ( fullname, sizeof ( fullname ), "%s%c%s", testnet->dev.name,
		   ( strchr ( name, '/' ) ? '.' : '/' ), name );

	/* Parse setting name */
	okx ( parse_setting_name ( fullname, autovivify_child_settings,
				   &settings, &setting ) == 0, file, line );

	/* Apply setting */
	okx ( storef_setting ( settings, &setting, value ) == 0, file, line );
}

/**
 * Report a network device closing test result
 *
 * @v testnet		Test network device
 * @v file		Test code file
 * @v line		Test code line
 */
void testnet_close_okx ( struct testnet *testnet, const char *file,
			unsigned int line ) {

	/* Sanity check */
	okx ( testnet->netdev != NULL, file, line );

	/* Close device */
	netdev_close ( testnet->netdev );
}

/**
 * Report a network device removal test result
 *
 * @v testnet		Test network device
 * @v file		Test code file
 * @v line		Test code line
 */
void testnet_remove_okx ( struct testnet *testnet, const char *file,
			  unsigned int line ) {

	/* Sanity check */
	okx ( testnet->netdev != NULL, file, line );

	/* Remove device */
	unregister_netdev ( testnet->netdev );
	netdev_nullify ( testnet->netdev );
	netdev_put ( testnet->netdev );
	testnet->netdev = NULL;
}
