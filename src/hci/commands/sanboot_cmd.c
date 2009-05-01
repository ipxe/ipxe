#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <gpxe/command.h>
#include <usr/autoboot.h>

FILE_LICENCE ( GPL2_OR_LATER );

/**
 * "sanboot" command syntax message
 *
 * @v argv		Argument list
 */
static void sanboot_syntax ( char **argv ) {
	printf ( "Usage:\n"
		 "  %s <root-path>\n"
		 "\n"
		 "Boot from SAN target\n",
		 argv[0] );
}

/**
 * The "sanboot" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int sanboot_exec ( int argc, char **argv ) {
	static struct option longopts[] = {
		{ "help", 0, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};
	const char *root_path = NULL;
	int c;
	int rc;

	/* Parse options */
	while ( ( c = getopt_long ( argc, argv, "h", longopts, NULL ) ) >= 0 ){
		switch ( c ) {
		case 'h':
			/* Display help text */
		default:
			/* Unrecognised/invalid option */
			sanboot_syntax ( argv );
			return 1;
		}
	}

	/* Need exactly one image name remaining after the options */
	if ( optind != ( argc - 1 ) ) {
		sanboot_syntax ( argv );
		return 1;
	}
	root_path = argv[optind];

	/* Boot from root path */
	if ( ( rc = boot_root_path ( root_path ) ) != 0 ) {
		printf ( "Could not boot from %s: %s\n",
			 root_path, strerror ( rc ) );
		return 1;
	}

	return 0;
}

struct command sanboot_command __command = {
	.name = "sanboot",
	.exec = sanboot_exec,
};
