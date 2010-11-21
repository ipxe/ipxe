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

#include <stdio.h>
#include <getopt.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <ipxe/netdevice.h>
#include <usr/autoboot.h>

FILE_LICENCE ( GPL2_OR_LATER );

/** @file
 *
 * Booting commands
 *
 */

/** "autoboot" options */
struct autoboot_options {};

/** "autoboot" option list */
static struct option_descriptor autoboot_opts[] = {};

/** "autoboot" command descriptor */
static struct command_descriptor autoboot_cmd =
	COMMAND_DESC ( struct autoboot_options, autoboot_opts, 0, 0,
		       "",
		       "Attempt to boot the system" );

/**
 * "autoboot" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int autoboot_exec ( int argc, char **argv ) {
	struct autoboot_options opts;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &autoboot_cmd, &opts ) ) != 0 )
		return rc;

	/* Try to boot */
	if ( ( rc = autoboot() ) != 0 )
		return rc;

	return 0;
}

/** "netboot" options */
struct netboot_options {};

/** "netboot" option list */
static struct option_descriptor netboot_opts[] = {};

/** "netboot" command descriptor */
static struct command_descriptor netboot_cmd =
	COMMAND_DESC ( struct netboot_options, netboot_opts, 1, 1,
		       "<interface>",
		       "Attempt to boot the system from <interface>" );

/**
 * "netboot" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int netboot_exec ( int argc, char **argv ) {
	struct netboot_options opts;
	struct net_device *netdev;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &netboot_cmd, &opts ) ) != 0 )
		return rc;

	/* Parse interface */
	if ( ( rc = parse_netdev ( argv[optind], &netdev ) ) != 0 )
		return rc;

	/* Try to boot */
	if ( ( rc = netboot ( netdev ) ) != 0 )
		return rc;

	return 0;
}

/** Booting commands */
struct command autoboot_commands[] __command = {
	{
		.name = "autoboot",
		.exec = autoboot_exec,
	},
	{
		.name = "netboot",
		.exec = netboot_exec,
	},
};
