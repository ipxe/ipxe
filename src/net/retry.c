/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stddef.h>
#include <latch.h>
#include <gpxe/list.h>
#include <gpxe/process.h>
#include <gpxe/init.h>
#include <gpxe/retry.h>

/** @file
 *
 * Retry timers
 *
 * A retry timer is a truncated binary exponential backoff timer.  It
 * can be used to build automatic retransmission into network
 * protocols.
 */

/** List of running timers */
static LIST_HEAD ( timers );

/**
 * Reload timer
 *
 * @v timer		Retry timer
 *
 * This reloads the timer with a new expiry time.  The expiry time
 * will be the timer's base timeout value, shifted left by the number
 * of retries (i.e. the number of timer expiries since the last timer
 * reset).
 */
static void reload_timer ( struct retry_timer *timer ) {
	unsigned int exp;

	exp = timer->retries;
	if ( exp > BACKOFF_LIMIT )
		exp = BACKOFF_LIMIT;
	timer->expiry = currticks() + ( timer->base << exp );
}

/**
 * Reset timer
 *
 * @v timer		Retry timer
 *
 * This resets the timer, i.e. clears its retry count and starts it
 * running with its base timeout value.
 *
 * Note that it is explicitly permitted to call reset_timer() on an
 * inactive timer.
 */
void reset_timer ( struct retry_timer *timer ) {
	timer->retries = 0;
	reload_timer ( timer );
}

/**
 * Start timer
 *
 * @v timer		Retry timer
 *
 * This resets the timer and starts it running (i.e. adds it to the
 * list of running timers).  The retry_timer::base and
 * retry_timer::callback fields must have been filled in.
 */
void start_timer ( struct retry_timer *timer ) {
	list_add ( &timer->list, &timers );
	reset_timer ( timer );
}

/**
 * Stop timer
 *
 * @v timer		Retry timer
 *
 * This stops the timer (i.e. removes it from the list of running
 * timers).
 */
void stop_timer ( struct retry_timer *timer ) {
	list_del ( &timer->list );
}

/**
 * Single-step the retry timer list
 *
 * @v process		Retry timer process
 */
static void retry_step ( struct process *process ) {
	struct retry_timer *timer;
	struct retry_timer *tmp;
	unsigned long now = currticks();

	list_for_each_entry_safe ( timer, tmp, &timers, list ) {
		if ( timer->expiry >= now ) {
			timer->retries++;
			reload_timer ( timer );
			timer->expired ( timer );
		}
	}

	schedule ( process );
}

/** Retry timer process */
static struct process retry_process = {
	.step = retry_step,
};

/** Initialise the retry timer module */
static void init_retry ( void ) {
	schedule ( &retry_process );
}

INIT_FN ( INIT_PROCESS, init_retry, NULL, NULL );
