#ifndef _GPXE_COMMAND_H
#define _GPXE_COMMAND_H

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/tables.h>

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

#define __command __table_entry ( COMMANDS, 01 )

#endif /* _GPXE_COMMAND_H */
