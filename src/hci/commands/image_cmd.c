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

#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <vsprintf.h>
#include <gpxe/image.h>
#include <gpxe/command.h>
#include <usr/fetch.h>
#include <usr/imgmgmt.h>

/** @file
 *
 * Image management commands
 *
 */

/**
 * Print image description
 *
 */

/**
 * "fetch"/"module"/"kernel" command syntax message
 *
 * @v argv		Argument list
 */
static void fetch_syntax ( char **argv ) {
	printf ( "Usage:\n"
		 "  %s [-n|--name <name>] filename [arguments...]\n"
		 "\n"
		 "Fetch executable/loadable image\n",
		 argv[0] );
}

/**
 * The "fetch"/"module"/"kernel" command body
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @v name		Default name for image, or NULL
 * @ret rc		Exit code
 */
static int fetch_exec_name ( int argc, char **argv, const char *name ) {
	static struct option longopts[] = {
		{ "help", 0, NULL, 'h' },
		{ "name", required_argument, NULL, 'n' },
		{ NULL, 0, NULL, 0 },
	};
	struct image *image;
	const char *filename;
	char cmdline[ sizeof ( image->cmdline ) ];
	size_t used = 0;
	int c;
	int rc;

	/* Parse options */
	while ( ( c = getopt_long ( argc, argv, "hn:",
				    longopts, NULL ) ) >= 0 ) {
		switch ( c ) {
		case 'n':
			/* Set image name */
			name = optarg;
			break;
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			fetch_syntax ( argv );
			return 1;
		}
	}

	/* Need at least a filename remaining after the options */
	if ( optind >= argc ) {
		fetch_syntax ( argv );
		return 1;
	}
	filename = argv[optind++];

	/* Build command line */
	while ( ( used < sizeof ( cmdline ) ) && ( optind < argc ) ) {
		used += snprintf ( &cmdline[used], sizeof ( cmdline ) - used,
				   " %s",  argv[optind++] );
	}

	/* Allocate and fill struct image */
	image = malloc ( sizeof ( *image ) );
	if ( ! image ) {
		printf ( "Out of memory\n" );
		return 1;
	}
	memset ( image, 0, sizeof ( *image ) );
	if ( name )
		strncpy ( image->name, name, ( sizeof ( image->name ) - 1 ) );
	if ( used )
		memcpy ( image->cmdline, cmdline, sizeof ( image->cmdline ) );

	/* Fetch the file */
	if ( ( rc = fetch ( image, filename ) ) != 0 ) {
		printf ( "Could not fetch %s: %s\n", filename,
			 strerror ( rc ) );
		free ( image );
		return 1;
	}

	/* Register the image */
	if ( ( rc = register_image ( image ) ) != 0 ) {
		printf ( "Could not register %s: %s\n", filename,
			 strerror ( rc ) );
		free ( image );
		return 1;
	}

	imgstat ( image );
	return 0;
}

/**
 * The "fetch"/"module" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int fetch_exec ( int argc, char **argv ) {
	return fetch_exec_name ( argc, argv, NULL );
}

/**
 * The "kernel" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int kernel_exec ( int argc, char **argv ) {
	return fetch_exec_name ( argc, argv, "kernel" );
}

/**
 * "imgstat" command syntax message
 *
 * @v argv		Argument list
 */
static void imgstat_syntax ( char **argv ) {
	printf ( "Usage:\n"
		 "  %s\n"
		 "\n"
		 "List executable/loadable images\n",
		 argv[0] );
}

/**
 * The "imgstat" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int imgstat_exec ( int argc __unused, char **argv __unused ) {
	static struct option longopts[] = {
		{ "help", 0, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};
	struct image *image;
	int c;

	/* Parse options */
	while ( ( c = getopt_long ( argc, argv, "h", longopts, NULL ) ) >= 0 ){
		switch ( c ) {
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			imgstat_syntax ( argv );
			return 1;
		}
	}

	/* Need at least a filename remaining after the options */
	if ( optind != argc ) {
		imgstat_syntax ( argv );
		return 1;
	}

	/* Show status of all images */
	for_each_image ( image ) {
		imgstat ( image );
	}
	return 0;
}

/** Image management commands */
struct command image_commands[] __command = {
	{
		.name = "fetch",
		.exec = fetch_exec,
	},
	{
		.name = "module",
		.exec = fetch_exec, /* synonym for "fetch" */
	},
	{
		.name = "kernel",
		.exec = kernel_exec,
	},
	{
		.name = "imgstat",
		.exec = imgstat_exec,
	},
};
