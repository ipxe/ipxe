#include "command.h"
#include "console.h"
#include <string.h>
#include <gpxe/tables.h>

static struct command cmd_start[0] __table_start ( commands );
static struct command cmd_end[0] __table_end ( commands );

void help_req(){}

static int cmd_help_exec ( int argc, char **argv ) {

	struct command *ccmd;
	int unknown = 1;
	if(argc == 1){
		printf("Available commands:\n\n  exit - Exit the command line and boot\n");

		for ( ccmd = cmd_start ; ccmd < cmd_end ; ccmd++ ) {
			printf ("  %s - %s\n", ccmd->name, ccmd->desc );
		}
	}else{
		if(!strcmp(argv[1], "exit") || !strcmp(argv[1], "quit")){
			printf("exit - Exit the command line and boot\n\nUsage:\n  exit\n");
		}else{
			for ( ccmd = cmd_start ; ccmd < cmd_end ; ccmd++ ) {
				if(!strcmp(ccmd->name, argv[1])){
					unknown = 0;
					printf ("%s - %s\n\nUsage:\n  %s\n", ccmd->name, ccmd->desc, ccmd->usage );
					break;
				}
			}
			if(unknown){
				printf("\"%s\" isn't compiled in (does it exist?).\n", argv[1]);
			}
		}
		
	}
	return 0;
}

struct command help_command __command = {
	.name = "help",
	.usage = "help <command>\n\nExample:\n  help help\n",
	.desc = "The help command",
	.exec = cmd_help_exec,
};


