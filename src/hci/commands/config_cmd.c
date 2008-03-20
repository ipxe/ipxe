#include <string.h>
#include <stdio.h>
#include <gpxe/command.h>
#include <gpxe/settings.h>
#include <gpxe/settings_ui.h>

static int config_exec ( int argc, char **argv ) {
	struct settings *settings;
	int rc;

	if ( argc > 2 ) {
		printf ( "Usage: %s [scope]\n"
			 "Opens the option configuration console\n", argv[0] );
		return 1;
	}

	if ( argc == 2 ) {
		settings = find_settings ( argv[1] );
		if ( ! settings ) {
			printf ( "No such scope \"%s\"\n", argv[1] );
			return 1;
		}
	} else {
		settings = &interactive_settings;
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
