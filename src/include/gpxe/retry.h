#ifndef _GPXE_RETRY_H
#define _GPXE_RETRY_H

/** @file
 *
 * Retry timers
 *
 */

#include <gpxe/list.h>

/** Effective maximum retry count for exponential backoff calculation */
#define BACKOFF_LIMIT 5

/** A retry timer */
struct retry_timer {
	/** List of active timers */
	struct list_head list;
	/** Base timeout (in ticks) */
	unsigned int base;
	/** Retry count */
	unsigned int retries;
	/** Expiry time (in ticks) */
	unsigned long expiry;
	/** Timer expired callback
	 *
	 * @v timer	Retry timer
	 */
	void ( * expired ) ( struct retry_timer *timer );
};

extern void start_timer ( struct retry_timer *timer );
extern void reset_timer ( struct retry_timer *timer );
extern void stop_timer ( struct retry_timer *timer );

#endif /* _GPXE_RETRY_H */
