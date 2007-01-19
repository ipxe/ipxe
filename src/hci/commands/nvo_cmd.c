#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <gpxe/nvo.h>
#include <gpxe/dhcp.h>
#include <gpxe/settings.h>
#include <gpxe/command.h>

extern struct nvo_block *ugly_nvo_hack;

static int show_exec ( int argc, char **argv ) {
	struct config_context dummy_context;
	char buf[256];
	int rc;

	if ( ! ugly_nvo_hack ) {
		printf ( "No non-volatile option storage available\n" );
		return 1;
	}

	if ( argc != 2 ) {
		printf ( "Syntax: %s <identifier>\n", argv[0] );
		return 1;
	}

	dummy_context.options = ugly_nvo_hack->options;
	if ( ( rc = show_named_setting ( &dummy_context, argv[1], buf,
					 sizeof ( buf ) ) ) != 0 ) {
		printf ( "Could not find \"%s\": %s\n",
			 argv[1], strerror ( -rc ) );
		return 1;
	}

	printf ( "%s = %s\n", argv[1], buf );
	return 0;
}

static int set_exec ( int argc, char **argv ) {
	struct config_context dummy_context;
	int rc;

	if ( ! ugly_nvo_hack ) {
		printf ( "No non-volatile option storage available\n" );
		return 1;
	}

	if ( argc != 3 ) {
		printf ( "Syntax: %s <identifier> <value>\n",
			 argv[0] );
		return 1;
	}

	dummy_context.options = ugly_nvo_hack->options;
	if ( ( rc = set_named_setting ( &dummy_context, argv[1],
					argv[2] ) ) != 0 ) {
		printf ( "Could not set \"%s\"=\"%s\": %s\n",
			 argv[1], argv[2], strerror ( -rc ) );
		return 1;
	}
	
	if ( nvo_save ( ugly_nvo_hack ) != 0 ) {
		printf ( "Could not save options to non-volatile storage\n" );
		return 1;
	}

	return 0;
}

static int clear_exec ( int argc, char **argv ) {
	struct config_context dummy_context;
	int rc;

	if ( ! ugly_nvo_hack ) {
		printf ( "No non-volatile option storage available\n" );
		return 1;
	}

	if ( argc != 2 ) {
		printf ( "Syntax: %s <identifier>\n",
			 argv[0] );
		return 1;
	}

	dummy_context.options = ugly_nvo_hack->options;
	if ( ( rc = clear_named_setting ( &dummy_context, argv[1] ) ) != 0 ) {
		printf ( "Could not clear \"%s\": %s\n",
			 argv[1], strerror ( -rc ) );
		return 1;
	}
	
	return 0;
}

struct command nvo_commands[] __command = {
	{
		.name = "show",
		.exec = show_exec,
	},
	{
		.name = "set",
		.exec = set_exec,
	},	
	{
		.name = "clear",
		.exec = clear_exec,
	},
};
