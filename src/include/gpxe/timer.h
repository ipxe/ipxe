#ifndef	GPXE_TIMER_H
#define GPXE_TIMER_H

#include <stddef.h>

typedef unsigned long tick_t;

#define MSECS_IN_SEC (1000)
#define USECS_IN_SEC (1000*1000)
#define USECS_IN_MSEC (1000)

#define	TICKS_PER_SEC	USECS_IN_SEC

extern tick_t currticks ( void );

extern void generic_currticks_udelay ( unsigned int usecs );

/** A timer */
struct timer {
	/** Initialise timer
	 *
	 * @ret rc	Return status code
	 */
	int ( * init ) ( void );
	/** Read current time
	 *
	 * @ret ticks	Current time, in ticks
	 */
	tick_t ( * currticks ) ( void );
	/** Delay
	 *
	 * @v usecs	Time to delay, in microseconds
	 */
	void ( * udelay ) ( unsigned int usecs );
};

#define __timer( order ) __table ( struct timer, timers, order )

#endif	/* GPXE_TIMER_H */

