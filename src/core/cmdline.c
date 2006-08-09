#include "cmdline.h"
#include "cmdlinelib.h"
#include <console.h>

void cmdl_start()
{
	cmd_line* cmd;
	
	cmd = cmdl_create();
	
	cmdl_setpropmt(cmd, "?>");

	cmdl_printf(cmd, "Welcome to Etherboot\n\n");
	
	while(!cmdl_getexit(cmd)){
		int i;
		
		printf("%s%s %s", cmdl_getoutput(cmd), cmdl_getprompt(cmd), cmdl_getbuffer(cmd));
		
		cmdl_addchar(cmd, getchar());
		
		/* TODO HACK temporary clear line */
		putchar(0xd);
		for(i=0; i < 79; i++){
			putchar(0x20);
		}
		putchar(0xd);
	}
	cmdl_free(cmd);
}

