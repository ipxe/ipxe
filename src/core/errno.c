#include "errno.h"
#include "vsprintf.h"

/* Global "last error" number */
int errno;

static struct errortab errortab_start[0] __table_start(errortab);
static struct errortab errortab_end[0] __table_end(errortab);

/*
 * Retrieve string representation of error number.
 *
 * If error not found in the error table, generate a generic "Error
 * 0x0000" message.
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
