#include <string.h>
#include <stdio.h>
#include <gpxe/command.h>
#include <gpxe/settings.h>
#include <gpxe/settings_ui.h>


#include <gpxe/nvo.h>
extern struct nvo_block *ugly_nvo_hack;


static int config_exec ( int argc, char **argv ) {
	struct config_context dummy_context;
	int rc;

	if ( argc != 1 ) {
		printf ( "Usage: %s\n"
			 "Opens the option configuration console\n", argv[0] );
		return 1;
	}

	if ( ! ugly_nvo_hack ) {
		printf ( "No non-volatile option storage available\n" );
		return 1;
	}

	dummy_context.options = ugly_nvo_hack->options;
	if ( ( rc = settings_ui ( &dummy_context ) ) != 0 ) {
		printf ( "Could not save settings: %s\n",
			 strerror ( rc ) );
		return 1;
	}

	return 0;
}

struct command config_command __command = {
	.name = "config",
	.exec = config_exec,
};
