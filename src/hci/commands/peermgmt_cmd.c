/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
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
FILE_SECBOOT ( PERMITTED );

#include <stdio.h>
#include <getopt.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <usr/peermgmt.h>

/** @file
 *
 * PeerDist management commands
 *
 */

/** "peerstat" options */
struct peerstat_options {};

/** "peerstat" option list */
static struct option_descriptor peerstat_opts[] = {};

/** "peerstat" command descriptor */
static struct command_descriptor peerstat_cmd =
	COMMAND_DESC ( struct peerstat_options, peerstat_opts, 0, 0, NULL );

/**
 * The "peerstat" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int peerstat_exec ( int argc, char **argv ) {
	struct peerstat_options opts;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &peerstat_cmd, &opts ) ) != 0 )
		return rc;

	peerstat();

	return 0;
}

/** Profiling commands */
COMMAND ( peerstat, peerstat_exec );
