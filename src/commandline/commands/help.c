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
		printf("Built in commands:\n\n\texit, quit\t\tExit the command line and boot\n\nCompiled in commands:\n\n");

		for ( ccmd = cmd_start ; ccmd < cmd_end ; ccmd++ ) {
			printf ("\t%s\t\t%s\n", ccmd->name, ccmd->desc );
		}
	}else{
		if(!strcmp(argv[1], "exit") || !strcmp(argv[1], "quit")){
			printf("exit, quit - The quit command\n\nUsage:\nquit or exit\n\n\tExample:\n\t\texit\n");
		}else{
			for ( ccmd = cmd_start ; ccmd < cmd_end ; ccmd++ ) {
				if(!strcmp(ccmd->name, argv[1])){
					unknown = 0;
					printf ("\t%s - %s\n\nUsage:\n%s\n", ccmd->name, ccmd->desc, ccmd->usage );
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
	.usage = "help <command>\n\n\tExample:\n\t\thelp help\n",
	.desc = "The help command",
	.exec = cmd_help_exec,
};


