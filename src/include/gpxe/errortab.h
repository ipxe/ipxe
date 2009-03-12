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

#define ERRORTAB "errortab"

#define __errortab __table ( struct errortab, ERRORTAB, 01 )

#endif /* _GPXE_ERRORTAB_H */
