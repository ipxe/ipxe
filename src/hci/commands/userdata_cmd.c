#include <errno.h>
#include <getopt.h>
#include <ipxe/command.h>
#include <ipxe/http.h>
#include <ipxe/parseopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <usr/userdata.h>

/** @file
 *
 * userdata commands
 *
 */

/** "userdata" options */
struct userdata_options {
	/** Use the ipv4 IMDS address **/
	int ipv4;
	/** Use the ipv6 IMDS address **/
	int ipv6;
};

/** "userdata" option list */
static struct option_descriptor userdata_opts[] = {
	OPTION_DESC ( "ipv4", '4', no_argument,
				  struct userdata_options, ipv4, parse_flag ),
	OPTION_DESC ( "ipv6", '6', no_argument,
				  struct userdata_options, ipv6, parse_flag ) };

/** "userdata" command descriptor */
static struct command_descriptor userdata_cmd =
	COMMAND_DESC ( struct userdata_options, userdata_opts, 0, 0, NULL );

/**
 * Execute userdata command from command line
 *
 * @v argc         Argument count
 * @v argv         Argument list
 * @ret rc         Return status code
 */
static int userdata_exec ( int argc, char **argv ) {
	struct userdata_options opts;
	struct image *image;
	int rc;

	/* Parse options */
	rc = parse_options ( argc, argv, &userdata_cmd, &opts );
	if ( rc != 0 )
		goto err_parse_options;

	/* Check for invalid flag combination */
	if ( opts.ipv4 && opts.ipv6 ) {
		printf ( "Error: Cannot specify both IPv4 and IPv6 flags\n" );
		rc = -EINVAL;
		goto err_parse_options;
	}

	/* Get user data based on IP version preference */
	if ( opts.ipv4 || opts.ipv6 ) {
		rc = get_userdata ( opts.ipv6, &image );
	} else {
		/* If no IP version is specified, try IPv4 first, then fall back to IPv6 */
		rc = get_userdata ( 0, &image );
		if ( rc != 0 ) {
			rc = get_userdata ( 1, &image );
		}
	}

	if ( rc != 0 ) {
		goto err_userdata;
	}

	/* Execute user data */
	rc = execute_userdata ( image );
	if ( rc != 0 )
		goto err_userdata;

err_userdata:
	image_put ( image );
err_parse_options:
	return rc;
}

/** userdata command */
struct command userdata_command __command = {
	.name = "userdata",
	.exec = userdata_exec,
};
