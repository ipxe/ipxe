#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <vsprintf.h>
#include <command.h>
#include <gpxe/nvo.h>
#include <gpxe/dhcp.h>

void nvo_cmd_req() {}

extern struct nvo_block *ugly_nvo_hack;

static int show_exec ( int argc, char **argv ) {

	if ( ! ugly_nvo_hack ) {
		printf ( "No non-volatile option storage available\n" );
		return 1;
	}

	hex_dump ( ugly_nvo_hack->options->data,
		   ugly_nvo_hack->options->len );
}

struct command show_command __command = {
	.name = "show",
	.usage = "show\n",
	.desc = "Show stored options",
	.exec = show_exec,
};

static int set_exec ( int argc, char **argv ) {
	unsigned long tag;
	struct dhcp_option *option;

	if ( ! ugly_nvo_hack ) {
		printf ( "No non-volatile option storage available\n" );
		return 1;
	}

	if ( argc != 3 ) {
		printf ( "Syntax: %s <option number> <option string>\n",
			 argv[0] );
		return 1;
	}

	tag = strtoul ( argv[1], NULL, 0 );
	option = set_dhcp_option ( ugly_nvo_hack->options, tag, argv[2],
				   strlen ( argv[2] ) );
	if ( ! option ) {
		printf ( "Could not set option %ld\n", tag );
		return 1;
	}

	if ( nvo_save ( ugly_nvo_hack ) != 0 ) {
		printf ( "Could not save options to non-volatile storage\n" );
		return 1;
	}

	return 0;
}

struct command set_command __command = {
	.name = "set",
	.usage = "set <option number> <option string>\n",
	.desc = "Set stored option",
	.exec = set_exec,
};
