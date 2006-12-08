#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>
#include <stdarg.h>

extern int execv ( const char *command, char * const argv[] );

/**
 * Execute command
 *
 * @v command		Command name
 * @v arg ...		Argument list (starting with argv[0])
 * @ret rc		Command exit status
 *
 * This is a front end to execv().
 */
#define execl( command, arg, ... ) ( {					\
		char * const argv[] = { (arg), ## __VA_ARGS__ };	\
		int rc = execv ( (command), argv );			\
		rc;							\
	} )

#endif /* _UNISTD_H */
