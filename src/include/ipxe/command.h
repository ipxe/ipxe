#ifndef _IPXE_COMMAND_H
#define _IPXE_COMMAND_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <ipxe/tables.h>

/** A command-line command */
struct command {
	/** Name of the command */
	const char *name;
	/**
	 * Function implementing the command
	 *
	 * @v argc		Argument count
	 * @v argv		Argument list
	 * @ret rc		Return status code
	 */
	int ( * exec ) ( int argc, char **argv );
};

#define COMMANDS __table ( struct command, "commands" )

#define __command( name ) __table_entry ( COMMANDS, _C2 ( 01., name ) )

#define COMMAND( name, exec )						\
	struct command name ## _command __command ( name ) = {		\
		#name, exec						\
	}

extern char * concat_args ( char **args );

#endif /* _IPXE_COMMAND_H */
