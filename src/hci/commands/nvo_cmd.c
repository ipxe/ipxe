#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <gpxe/settings.h>
#include <gpxe/command.h>

FILE_LICENCE ( GPL2_OR_LATER );

static int show_exec ( int argc, char **argv ) {
	char buf[256];
	int rc;

	if ( argc != 2 ) {
		printf ( "Syntax: %s <identifier>\n", argv[0] );
		return 1;
	}

	if ( ( rc = fetchf_named_setting ( argv[1], buf,
					   sizeof ( buf ) ) ) < 0 ){
		printf ( "Could not find \"%s\": %s\n",
			 argv[1], strerror ( rc ) );
		return 1;
	}

	printf ( "%s = %s\n", argv[1], buf );
	return 0;
}

static int set_exec ( int argc, char **argv ) {
	int rc;

	if ( argc != 3 ) {
		printf ( "Syntax: %s <identifier> <value>\n", argv[0] );
		return 1;
	}

	if ( ( rc = storef_named_setting ( argv[1], argv[2] ) ) != 0 ) {
		printf ( "Could not set \"%s\"=\"%s\": %s\n",
			 argv[1], argv[2], strerror ( rc ) );
		return 1;
	}

	return 0;
}

static int clear_exec ( int argc, char **argv ) {
	int rc;

	if ( argc != 2 ) {
		printf ( "Syntax: %s <identifier>\n", argv[0] );
		return 1;
	}

	if ( ( rc = delete_named_setting ( argv[1] ) ) != 0 ) {
		printf ( "Could not clear \"%s\": %s\n",
			 argv[1], strerror ( rc ) );
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
