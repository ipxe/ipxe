#ifndef COMMAND_H
#define COMMAND_H

#include <gpxe/tables.h>

struct command {
	const char *name;	 	     				// The name of the command
	const char *usage;						// Description of how to use the command
	const char *desc;						// Short description of the command
	int ( *exec ) ( int argc, char **argv);				// The command function to call
};

#define __command __table ( commands, 01 )
#endif

