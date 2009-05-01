#include <stdio.h>
#include <gpxe/command.h>
#include <usr/autoboot.h>

FILE_LICENCE ( GPL2_OR_LATER );

static int autoboot_exec ( int argc, char **argv ) {

	if ( argc != 1 ) {
		printf ( "Usage:\n"
			 "  %s\n"
			 "\n"
			 "Attempts to boot the system\n",
			 argv[0] );
		return 1;
	}

	autoboot();

	/* Can never return success by definition */
	return 1;
}

struct command autoboot_command __command = {
	.name = "autoboot",
	.exec = autoboot_exec,
};
