#ifndef _GPXE_ERRORTAB_H
#define _GPXE_ERRORTAB_H

/** @file
 *
 * Error message tables
 *
 */

#include <gpxe/tables.h>

struct errortab {
	int errno;
	const char *text;
};

#define __errortab __table ( struct errortab, errortab, 01 )

#endif /* _GPXE_ERRORTAB_H */
