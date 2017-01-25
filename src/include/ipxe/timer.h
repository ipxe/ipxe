#ifndef	_IPXE_TIMER_H
#define _IPXE_TIMER_H

/** @file
 *
 * iPXE timers
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/tables.h>

/** Number of ticks per second */
#define TICKS_PER_SEC 1024

/** Number of ticks per millisecond
 *
 * This is (obviously) not 100% consistent with the definition of
 * TICKS_PER_SEC, but it allows for multiplications and divisions to
 * be elided.  In any case, timer ticks are not expected to be a
 * precision timing source; for example, the standard BIOS timer is
 * based on an 18.2Hz clock.
 */
#define TICKS_PER_MS 1

/** A timer */
struct timer {
	/** Name */
	const char *name;
	/**
	 * Probe timer
	 *
	 * @ret rc		Return status code
	 */
	int ( * probe ) ( void );
	/**
	 * Get current system time in ticks
	 *
	 * @ret ticks		Current time, in ticks
	 */
	unsigned long ( * currticks ) ( void );
	/**
	 * Delay for a fixed number of microseconds
	 *
	 * @v usecs		Number of microseconds for which to delay
	 */
	void ( * udelay ) ( unsigned long usecs );
};

/** Timer table */
#define TIMERS __table ( struct timer, "timers" )

/** Declare a timer */
#define __timer( order ) __table_entry ( TIMERS, order )

/** @defgroup timer_order Timer detection order
 *
 * @{
 */

#define TIMER_PREFERRED	01	/**< Preferred timer */
#define TIMER_NORMAL	02	/**< Normal timer */

/** @} */

/*
 * sleep() prototype is defined by POSIX.1.  usleep() prototype is
 * defined by 4.3BSD.  udelay() and mdelay() prototypes are chosen to
 * be reasonably sensible.
 *
 */

extern void udelay ( unsigned long usecs );
extern void mdelay ( unsigned long msecs );
extern unsigned long currticks ( void );
extern unsigned int sleep ( unsigned int seconds );

#endif /* _IPXE_TIMER_H */
