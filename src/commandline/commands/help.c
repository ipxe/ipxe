#include <string.h>
#include <vsprintf.h>
#include <gpxe/tables.h>
#include <gpxe/command.h>

static struct command cmd_start[0] __table_start ( commands );
static struct command cmd_end[0] __table_end ( commands );

void help_req(){}

static int cmd_help_exec ( int argc, char **argv ) {

	struct command *ccmd;
	int unknown = 1;


	printf("Available commands:\n\n  exit - Exit the command line and boot\n");

	for ( ccmd = cmd_start ; ccmd < cmd_end ; ccmd++ ) {
		printf ("  %s\n", ccmd->name );
	}

	return 0;
}

struct command help_command __command = {
	.name = "help",
	.exec = cmd_help_exec,
};


