#ifndef _GPXE_COMMAND_H
#define _GPXE_COMMAND_H

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

#define __command __table ( struct command, commands, 01 )

#endif /* _GPXE_COMMAND_H */
