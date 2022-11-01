#ifndef _UNISTD_H
#define _UNISTD_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

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

/* Pick up udelay() and sleep() */
#include <ipxe/timer.h>

static inline __always_inline void usleep ( unsigned long usecs ) {
	udelay ( usecs );
}

#endif /* _UNISTD_H */
