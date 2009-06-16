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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <libgen.h>
#include <getopt.h>
#include <gpxe/image.h>
#include <gpxe/command.h>
#include <usr/imgmgmt.h>

/** @file
 *
 * Image management commands
 *
 */

enum image_action {
	IMG_FETCH = 0,
	IMG_LOAD,
	IMG_EXEC,
};

/**
 * Fill in image command line
 *
 * @v image		Image
 * @v nargs		Argument count
 * @v args		Argument list
 * @ret rc		Return status code
 */
static int imgfill_cmdline ( struct image *image, unsigned int nargs, 
			     char **args ) {
	size_t len;
	unsigned int i;

	/* Determine total length of command line */
	len = 1; /* NUL */
	for ( i = 0 ; i < nargs ; i++ )
		len += ( 1 /* possible space */ + strlen ( args[i] ) );

	{
		char buf[len];
		char *ptr = buf;

		/* Assemble command line */
		buf[0] = '\0';
		for ( i = 0 ; i < nargs ; i++ ) {
			ptr += sprintf ( ptr, "%s%s", ( i ? " " : "" ),
					 args[i] );
		}
		assert ( ptr < ( buf + len ) );

		return image_set_cmdline ( image, buf );
	}
}

/**
 * "imgfetch"/"module"/"kernel" command syntax message
 *
 * @v argv		Argument list
 */
static void imgfetch_core_syntax ( char **argv, enum image_action action ) {
	static const char *actions[] = {
		[IMG_FETCH]	= "Fetch",
		[IMG_LOAD]	= "Fetch and load",
		[IMG_EXEC]	= "Fetch and execute",
	};

	printf ( "Usage:\n"
		 "  %s [-n|--name <name>] filename [arguments...]\n"
		 "\n"
		 "%s executable/loadable image\n",
		 argv[0], actions[action] );
}

/**
 * The "imgfetch"/"module"/"kernel" command body
 *
 * @v image_type	Image type to assign (or NULL)
 * @v load		Image will be automatically loaded after fetching
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int imgfetch_core_exec ( struct image_type *image_type,
				enum image_action action,
				int argc, char **argv ) {
	static struct option longopts[] = {
		{ "help", 0, NULL, 'h' },
		{ "name", required_argument, NULL, 'n' },
		{ NULL, 0, NULL, 0 },
	};
	struct image *image;
	const char *name = NULL;
	char *filename;
	int ( * image_register ) ( struct image *image );
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
			imgfetch_core_syntax ( argv, action );
			return -EINVAL;
		}
	}

	/* Need at least a filename remaining after the options */
	if ( optind == argc ) {
		imgfetch_core_syntax ( argv, action );
		return -EINVAL;
	}
	filename = argv[optind++];
	if ( ! name )
		name = basename ( filename );

	/* Allocate image */
	image = alloc_image();
	if ( ! image ) {
		printf ( "%s\n", strerror ( -ENOMEM ) );
		return -ENOMEM;
	}

	/* Fill in image name */
	if ( name ) {
		if ( ( rc = image_set_name ( image, name ) ) != 0 )
			return rc;
	}

	/* Set image type (if specified) */
	image->type = image_type;

	/* Fill in command line */
	if ( ( rc = imgfill_cmdline ( image, ( argc - optind ),
				      &argv[optind] ) ) != 0 )
		return rc;

	/* Fetch the image */
	switch ( action ) {
	case IMG_FETCH:
		image_register = register_image;
		break;
	case IMG_LOAD:
		image_register = register_and_autoload_image;
		break;
	case IMG_EXEC:
		image_register = register_and_autoexec_image;
		break;
	default:
		assert ( 0 );
		return -EINVAL;
	}
	if ( ( rc = imgfetch ( image, filename, image_register ) ) != 0 ) {
		printf ( "Could not fetch %s: %s\n",
			 filename, strerror ( rc ) );
		image_put ( image );
		return rc;
	}

	image_put ( image );
	return 0;
}

/**
 * The "imgfetch"/"module" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int imgfetch_exec ( int argc, char **argv ) {
	int rc;

	if ( ( rc = imgfetch_core_exec ( NULL, IMG_FETCH,
					 argc, argv ) ) != 0 )
		return rc;

	return 0;
}

/**
 * The "kernel" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int kernel_exec ( int argc, char **argv ) {
	int rc;

	if ( ( rc = imgfetch_core_exec ( NULL, IMG_LOAD, argc, argv ) ) != 0 )
		return rc;

	return 0;
}

/**
 * The "chain" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int chain_exec ( int argc, char **argv) {
	int rc;

	if ( ( rc = imgfetch_core_exec ( NULL, IMG_EXEC, argc, argv ) ) != 0 )
		return rc;

	return 0;
}

/**
 * "imgload" command syntax message
 *
 * @v argv		Argument list
 */
static void imgload_syntax ( char **argv ) {
	printf ( "Usage:\n"
		 "  %s <image name>\n"
		 "\n"
		 "Load executable/loadable image\n",
		 argv[0] );
}

/**
 * The "imgload" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int imgload_exec ( int argc, char **argv ) {
	static struct option longopts[] = {
		{ "help", 0, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};
	struct image *image;
	const char *name;
	int c;
	int rc;

	/* Parse options */
	while ( ( c = getopt_long ( argc, argv, "h", longopts, NULL ) ) >= 0 ){
		switch ( c ) {
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			imgload_syntax ( argv );
			return 1;
		}
	}

	/* Need exactly one image name remaining after the options */
	if ( optind != ( argc - 1 ) ) {
		imgload_syntax ( argv );
		return 1;
	}
	name = argv[optind];

	/* Load all specified images */
	image = find_image ( name );
	if ( ! image ) {
		printf ( "No such image: %s\n", name );
		return 1;
	}
	if ( ( rc = imgload ( image ) ) != 0 ) {
		printf ( "Could not load %s: %s\n", name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * "imgargs" command syntax message
 *
 * @v argv		Argument list
 */
static void imgargs_syntax ( char **argv ) {
	printf ( "Usage:\n"
		 "  %s <image name> [<arguments>...]\n"
		 "\n"
		 "Set arguments for executable/loadable image\n",
		 argv[0] );
}

/**
 * The "imgargs" command body
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int imgargs_exec ( int argc, char **argv ) {
	static struct option longopts[] = {
		{ "help", 0, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};
	struct image *image;
	const char *name;
	int c;
	int rc;

	/* Parse options */
	while ( ( c = getopt_long ( argc, argv, "h", longopts, NULL ) ) >= 0 ){
		switch ( c ) {
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			imgargs_syntax ( argv );
			return 1;
		}
	}

	/* Need at least an image name remaining after the options */
	if ( optind == argc ) {
		imgargs_syntax ( argv );
		return 1;
	}
	name = argv[optind++];

	/* Fill in command line */
	image = find_image ( name );
	if ( ! image ) {
		printf ( "No such image: %s\n", name );
		return 1;
	}
	if ( ( rc = imgfill_cmdline ( image, ( argc - optind ),
				      &argv[optind] ) ) != 0 )
		return rc;


	return 0;
}

/**
 * "imgexec" command syntax message
 *
 * @v argv		Argument list
 */
static void imgexec_syntax ( char **argv ) {
	printf ( "Usage:\n"
		 "  %s <image name>\n"
		 "\n"
		 "Execute executable/loadable image\n",
		 argv[0] );
}

/**
 * The "imgexec" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int imgexec_exec ( int argc, char **argv ) {
	static struct option longopts[] = {
		{ "help", 0, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};
	struct image *image;
	const char *name = NULL;
	int c;
	int rc;

	/* Parse options */
	while ( ( c = getopt_long ( argc, argv, "h", longopts, NULL ) ) >= 0 ){
		switch ( c ) {
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			imgexec_syntax ( argv );
			return 1;
		}
	}

	/* Need no more than one image name */
	if ( optind != argc )
		name = argv[optind++];
	if ( optind != argc ) {
		imgexec_syntax ( argv );
		return 1;
	}
	
	/* Execute specified image */
	if ( name ) {
		image = find_image ( name );
		if ( ! image ) {
			printf ( "No such image: %s\n", name );
			return 1;
		}
	} else {
		image = imgautoselect();
		if ( ! image ) {
			printf ( "No (unique) loaded image\n" );
			return 1;
		}
	}

	if ( ( rc = imgexec ( image ) ) != 0 ) {
		printf ( "Could not execute %s: %s\n",
			 image->name, strerror ( rc ) );
		return 1;
	}

	return 0;
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
static int imgstat_exec ( int argc, char **argv ) {
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

	/* No arguments */
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

/**
 * "imgstat" command syntax message
 *
 * @v argv		Argument list
 */
static void imgfree_syntax ( char **argv ) {
	printf ( "Usage:\n"
		 "  %s [<image name>]\n"
		 "\n"
		 "Free one or all executable/loadable images\n",
		 argv[0] );
}

/**
 * The "imgfree" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int imgfree_exec ( int argc, char **argv ) {
	static struct option longopts[] = {
		{ "help", 0, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};
	struct image *image;
	struct image *tmp;
	const char *name = NULL;
	int c;

	/* Parse options */
	while ( ( c = getopt_long ( argc, argv, "h", longopts, NULL ) ) >= 0 ){
		switch ( c ) {
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			imgfree_syntax ( argv );
			return 1;
		}
	}

	/* Need no more than one image name */
	if ( optind != argc )
		name = argv[optind++];
	if ( optind != argc ) {
		imgfree_syntax ( argv );
		return 1;
	}

	if ( name ) {
		/* Free specified image (may leak) */
		image = find_image ( name );
		if ( ! image ) {
			printf ( "No such image: %s\n", name );
			return 1;
		}
		imgfree ( image );
	} else {
		/* Free all images */
		list_for_each_entry_safe ( image, tmp, &images, list ) {
			imgfree ( image );
		}
	}
	return 0;
}

/** Image management commands */
struct command image_commands[] __command = {
	{
		.name = "imgfetch",
		.exec = imgfetch_exec,
	},
	{
		.name = "module",
		.exec = imgfetch_exec, /* synonym for "imgfetch" */
	},
	{
		.name = "initrd",
		.exec = imgfetch_exec, /* synonym for "imgfetch" */
	},
	{
		.name = "kernel",
		.exec = kernel_exec,
	},
	{
		.name = "chain",
		.exec = chain_exec,
	},
	{
		.name = "imgload",
		.exec = imgload_exec,
	},
	{
		.name = "imgargs",
		.exec = imgargs_exec,
	},
	{
		.name = "imgexec",
		.exec = imgexec_exec,
	},
	{
		.name = "boot", /* synonym for "imgexec" */
		.exec = imgexec_exec,
	},
	{
		.name = "imgstat",
		.exec = imgstat_exec,
	},
	{
		.name = "imgfree",
		.exec = imgfree_exec,
	},
};
