#ifndef	GPXE_TIMER_H
#define GPXE_TIMER_H

#include <stddef.h>

typedef uint32_t tick_t;

#define MSECS_IN_SEC (1000)
#define USECS_IN_SEC (1000*1000)
#define USECS_IN_MSEC (1000)

#define	TICKS_PER_SEC	USECS_IN_SEC

tick_t currticks(void);

void generic_currticks_udelay(unsigned int usecs);

struct timer {
	/* Returns zero on successful initialisation. */
	int (*init) (void);

	/* Return the current time, int mictoseconds since the beginning. */
	tick_t (*currticks) (void);

	/* Sleep for a few useconds. */
	void (*udelay) (unsigned int useconds);
};

#define __timer(order) __table (struct timer, timers, order)

#endif	/* GPXE_TIMER_H */

