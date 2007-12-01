#ifndef TIMER_H
#define TIMER_H

/*
 * This file should be removed as soon as there are no
 * currticks() abusers.
 */

#include <stddef.h>
/*
#warning Please fix me. I'm abusing the deprecated include/timer.h
*/
#include <unistd.h>

/* Duplicates include/gpxe/timer.h */
typedef uint32_t tick_t;

#define MSECS_IN_SEC (1000)
#define USECS_IN_SEC (1000*1000)
#define USECS_IN_MSEC (1000)

#define	TICKS_PER_SEC USECS_IN_SEC

tick_t currticks(void);

#endif

