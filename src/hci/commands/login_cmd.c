#include <string.h>
#include <stdio.h>
#include <gpxe/command.h>
#include <gpxe/login_ui.h>

FILE_LICENCE ( GPL2_OR_LATER );

static int login_exec ( int argc, char **argv ) {
	int rc;

	if ( argc > 1 ) {
		printf ( "Usage: %s\n"
			 "Prompt for login credentials\n", argv[0] );
		return 1;
	}

	if ( ( rc = login_ui() ) != 0 ) {
		printf ( "Could not set credentials: %s\n",
			 strerror ( rc ) );
		return 1;
	}

	return 0;
}

struct command login_command __command = {
	.name = "login",
	.exec = login_exec,
};
