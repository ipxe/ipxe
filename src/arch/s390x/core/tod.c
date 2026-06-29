/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Time-of-Day (TOD) Clock
 *
 */

#include <errno.h>
#include <ipxe/time.h>
#include <ipxe/timer.h>
#include <ipxe/facility.h>
#include <ipxe/tod.h>
#include <config/time.h>

#ifdef TIME_TOD
#define TIME_PREFIX_tod
#else
#define TIME_PREFIX_tod __tod_
#endif

struct timer tod_timer __timer ( TIMER_PREFERRED );

/** Colour for debug messages */
#define colour &tod_timer

/**
 * Get current wall-clock time in seconds since the Epoch
 *
 * @ret time		Time, in seconds
 */
static time_t tod_now ( void ) {
	unsigned long ticks;

	/* Get current time in TOD ticks */
	ticks = tod_ticks();

	/* Convert (exactly) to seconds since the Epoch */
	return ( ( ticks - TOD_EPOCH ) / ( 1000000 * TOD_TICKS_PER_US ) );
}

/**
 * Get current system time in ticks
 *
 * @ret ticks		Current time, in ticks
 */
static unsigned long tod_currticks ( void ) {
	unsigned long ticks;

	/* Get current time in TOD ticks */
	ticks = tod_ticks();

	/* Convert (approximately) to system ticks */
	return ( ticks / ( TOD_TICKS_PER_MS / TICKS_PER_MS ) );
}

/**
 * Delay for a fixed number of microseconds
 *
 * @v usecs		Number of microseconds for which to delay
 */
static void tod_udelay ( unsigned long usecs ) {
	unsigned long start;
	unsigned long elapsed;
	unsigned long threshold;

	/* Delay until sufficient time has elapsed */
	start = tod_ticks();
	threshold = ( usecs * TOD_TICKS_PER_US );
	do {
		elapsed = ( tod_ticks() - start );
	} while ( elapsed < threshold );
}

/**
 * Probe timer
 *
 * @ret rc		Return status code
 */
static int tod_probe ( void ) {
	enum tod_state state;
	int multiple;

	/* Check multiple-epoch facility (for debugging only) */
	multiple = facility_is_installed ( FACILITY_MULTIPLE_EPOCH );
	DBGC ( colour, "TOD multiple epoch facility is%s installed\n",
	       ( multiple ? "" : " not" ) );

	/* Get clock state */
	state = tod_state();
	DBGC ( colour, "TOD clock is in %s state\n",
	       tod_state_name ( state ) );

	/* Check clock is running */
	if ( ! tod_is_running ( state ) )
		return -ENODEV;

	return 0;
}

/** Time-of-day interval timer */
struct timer tod_timer __timer ( TIMER_PREFERRED ) = {
	.name = "tod",
	.probe = tod_probe,
	.currticks = tod_currticks,
	.udelay = tod_udelay,
};

PROVIDE_TIME ( tod, time_now, tod_now );
