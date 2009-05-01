#include <string.h>
#include <stdio.h>
#include <gpxe/command.h>
#include <gpxe/settings.h>
#include <gpxe/settings_ui.h>

FILE_LICENCE ( GPL2_OR_LATER );

static int config_exec ( int argc, char **argv ) {
	char *settings_name;
	struct settings *settings;
	int rc;

	if ( argc > 2 ) {
		printf ( "Usage: %s [scope]\n"
			 "Opens the option configuration console\n", argv[0] );
		return 1;
	}

	settings_name = ( ( argc == 2 ) ? argv[1] : "" );
	settings = find_settings ( settings_name );
	if ( ! settings ) {
		printf ( "No such scope \"%s\"\n", settings_name );
		return 1;
	}

	if ( ( rc = settings_ui ( settings ) ) != 0 ) {
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
