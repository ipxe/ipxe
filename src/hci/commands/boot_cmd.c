#include <vsprintf.h>
#include <gpxe/command.h>
#include <gpxe/autoboot.h>

static int boot_exec ( int argc, char **argv ) {

	if ( argc != 1 ) {
		printf ( "Usage: %s\n"
			 "Attempts to boot the system\n", argv[0] );
		return 1;
	}

	autoboot();

	return 0;
}

struct command boot_command __command = {
	.name = "boot",
	.exec = boot_exec,
};
