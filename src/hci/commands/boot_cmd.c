#include <vsprintf.h>
#include <gpxe/command.h>
#include <usr/autoboot.h>

static int boot_exec ( int argc, char **argv ) {

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

struct command boot_command __command = {
	.name = "boot",
	.exec = boot_exec,
};
