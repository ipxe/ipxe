#include "etherboot.h"
#include "errno.h"
#include "vsprintf.h"

/** @file
 *
 * Error codes and descriptions.
 *
 * This file provides the global variable errno
 *
 */

/**
 * Global "last error" number.
 *
 * This is valid only when a function has just returned indicating a
 * failure.
 *
 */
int errno;

static struct errortab errortab_start[0] __table_start(errortab);
static struct errortab errortab_end[0] __table_end(errortab);

/**
 * Retrieve string representation of error number.
 *
 * @v errno		Error number
 * @ret strerror	Pointer to error text
 *
 * If the error is not found in the linked-in error tables, generates
 * a generic "Error 0x0000" message.
 *
 * The pointer returned by strerror() is valid only until the next
 * call to strerror().
 *
 */
const char * strerror ( int errno ) {
	static char *generic_message = "Error 0x0000";
	struct errortab *errortab;

	for ( errortab = errortab_start ; errortab < errortab_end ;
	      errortab++ ) {
		if ( errortab->errno == errno )
			return errortab->text;
	}

	sprintf ( generic_message + 8, "%hx", errno );
	return generic_message;
}
