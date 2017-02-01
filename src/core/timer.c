/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <string.h>
#include <ipxe/process.h>
#include <ipxe/console.h>
#include <ipxe/keys.h>
#include <ipxe/nap.h>
#include <ipxe/init.h>
#include <ipxe/timer.h>

/** Current timer */
static struct timer *timer;

/**
 * Get current system time in ticks
 *
 * @ret ticks		Current time, in ticks
 */
unsigned long currticks ( void ) {

	/* Guard against use during early initialisation */
	if ( ! timer ) {
		DBGC ( &timer, "TIMER currticks() called before initialisation "
		       "from %p\n", __builtin_return_address ( 0 ) );
		return 0;
	}

	/* Use selected timer */
	return timer->currticks();
}

/**
 * Delay for a fixed number of microseconds
 *
 * @v usecs		Number of microseconds for which to delay
 */
void udelay ( unsigned long usecs ) {

	/* Guard against use during early initialisation */
	if ( ! timer ) {
		DBGC ( &timer, "TIMER udelay() called before initialisation "
		       "from %p\n", __builtin_return_address ( 0 ) );
		return;
	}

	/* Use selected timer */
	timer->udelay ( usecs );
}

/**
 * Delay for a fixed number of milliseconds
 *
 * @v msecs		Number of milliseconds for which to delay
 */
void mdelay ( unsigned long msecs ) {

	/* Guard against use during early initialisation */
	if ( ! timer ) {
		DBGC ( &timer, "TIMER mdelay() called before initialisation "
		       "from %p\n", __builtin_return_address ( 0 ) );
		return;
	}

	/* Delay for specified number of milliseconds */
	while ( msecs-- )
		udelay ( 1000 );
}

/**
 * Sleep (interruptibly) for a fixed number of seconds
 *
 * @v secs		Number of seconds for which to delay
 * @ret secs		Number of seconds remaining, if interrupted
 */
unsigned int sleep ( unsigned int secs ) {
	unsigned long start = currticks();
	unsigned long now;

	for ( ; secs ; secs-- ) {
		while ( ( ( now = currticks() ) - start ) < TICKS_PER_SEC ) {
			step();
			if ( iskey() && ( getchar() == CTRL_C ) )
				return secs;
			cpu_nap();
		}
		start = now;
	}

	return 0;
}

/**
 * Find a working timer
 *
 */
static void timer_probe ( void ) {
	int rc;

	/* Use first working timer */
	for_each_table_entry ( timer, TIMERS ) {
		if ( ( timer->probe == NULL ) ||
		     ( ( rc = timer->probe() ) == 0 ) ) {
			DBGC ( &timer, "TIMER using %s\n", timer->name );
			return;
		}
		DBGC ( &timer, "TIMER could not initialise %s: %s\n",
		       timer->name, strerror ( rc ) );
	}

	/* This is a fatal error */
	DBGC ( &timer, "TIMER found no working timers!\n" );
	while ( 1 ) {}
}

/** Timer initialisation function */
struct init_fn timer_init_fn __init_fn ( INIT_EARLY ) = {
	.initialise = timer_probe,
};

/* Drag in timer configuration */
REQUIRING_SYMBOL ( timer_init_fn );
REQUIRE_OBJECT ( config_timer );
