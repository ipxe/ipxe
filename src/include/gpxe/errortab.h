#ifndef _GPXE_ERRORTAB_H
#define _GPXE_ERRORTAB_H

/** @file
 *
 * Error message tables
 *
 */

#include <errno.h>
#include <gpxe/tables.h>

struct errortab {
	int errno;
	const char *text;
};

#define __errortab __table ( errortab, 01 )

#endif /* _GPXE_ERRORTAB_H */
