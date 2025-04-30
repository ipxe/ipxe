/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <getopt.h>
#include <ipxe/uaccess.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <usr/imgmgmt.h>

/** @file
 *
 * Read memory command
 *
 */

/** "imgmem" options */
struct imgmem_options {
	/** Image name */
	char *name;
};

/** "imgmem" option list */
static struct option_descriptor imgmem_opts[] = {
	OPTION_DESC ( "name", 'n', required_argument,
		      struct imgmem_options, name, parse_string ),
};

/** "imgmem" command descriptor */
static struct command_descriptor imgmem_cmd =
	COMMAND_DESC ( struct imgmem_options, imgmem_opts, 2, 2,
		       "<address> <length>" );

/**
 * The "imgmem" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int imgmem_exec ( int argc, char **argv ) {
	struct imgmem_options opts;
	unsigned int data;
	unsigned int len;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &imgmem_cmd, &opts ) ) != 0 )
		return rc;

	/* Use start address as name if none specified */
	if ( ! opts.name )
		opts.name = argv[optind];

	/* Parse address */
	if ( ( rc = parse_integer ( argv[optind++], &data ) ) != 0 )
		return rc;

	/* Parse length */
	if ( ( rc = parse_integer ( argv[optind++], &len ) ) != 0 )
		return rc;

	/* Create image */
	if ( ( rc = imgmem ( opts.name, phys_to_virt ( data ), len ) ) != 0 )
		return rc;

	return 0;
}

/** Read memory command */
struct command imgmem_commands[] __command = {
	{
		.name = "imgmem",
		.exec = imgmem_exec,
	},
};
