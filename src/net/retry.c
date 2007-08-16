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
 * A retry timer is a binary exponential backoff timer.  It can be
 * used to build automatic retransmission into network protocols.
 *
 * This implementation of the timer is designed to satisfy RFC 2988
 * and therefore be usable as a TCP retransmission timer.
 *
 * 
 */

/** Default timeout value */
#define MIN_TIMEOUT ( TICKS_PER_SEC / 4 )

/** Limit after which the timeout will be deemed permanent */
#define MAX_TIMEOUT ( 10 * TICKS_PER_SEC )

/* The theoretical minimum that the algorithm in stop_timer() can
 * adjust the timeout back down to is seven ticks, so set the minimum
 * timeout to at least that value for the sake of consistency.
 */
#if MIN_TIMEOUT < 7
#undef MIN_TIMEOUT
#define MIN_TIMEOUT 7
#endif

/** List of running timers */
static LIST_HEAD ( timers );

/**
 * Start timer
 *
 * @v timer		Retry timer
 *
 * This starts the timer running with the current timeout value.  If
 * stop_timer() is not called before the timer expires, the timer will
 * be stopped and the timer's callback function will be called.
 */
void start_timer ( struct retry_timer *timer ) {
	if ( ! timer_running ( timer ) )
		list_add ( &timer->list, &timers );
	timer->start = currticks();
	if ( timer->timeout < MIN_TIMEOUT )
		timer->timeout = MIN_TIMEOUT;
	DBG2 ( "Timer %p started at time %ld (expires at %ld)\n",
	       timer, timer->start, ( timer->start + timer->timeout ) );
}

/**
 * Start timer with no delay
 *
 * @v timer		Retry timer
 *
 * This starts the timer running with a zero timeout value.
 */
void start_timer_nodelay ( struct retry_timer *timer ) {
	start_timer ( timer );
	timer->timeout = 0;
}

/**
 * Stop timer
 *
 * @v timer		Retry timer
 *
 * This stops the timer and updates the timer's timeout value.
 */
void stop_timer ( struct retry_timer *timer ) {
	unsigned long old_timeout = timer->timeout;
	unsigned long now = currticks();
	unsigned long runtime;

	/* If timer was already stopped, do nothing */
	if ( ! timer_running ( timer ) )
		return;

	list_del ( &timer->list );
	runtime = ( now - timer->start );
	timer->start = 0;
	DBG2 ( "Timer %p stopped at time %ld (ran for %ld)\n",
	       timer, now, runtime );

	/* Update timer.  Variables are:
	 *
	 *   r = round-trip time estimate (i.e. runtime)
	 *   t = timeout value (i.e. timer->timeout)
	 *   s = smoothed round-trip time
	 *
	 * By choice, we set t = 4s, i.e. allow for four times the
	 * normal round-trip time to pass before retransmitting.
	 *
	 * We want to smooth according to s := ( 7 s + r ) / 8
	 *
	 * Since we don't actually store s, this reduces to
	 * t := ( 7 t / 8 ) + ( r / 2 )
	 *
	 */
	if ( timer->count ) {
		timer->count--;
	} else {
		timer->timeout -= ( timer->timeout >> 3 );
		timer->timeout += ( runtime >> 1 );
		if ( timer->timeout != old_timeout ) {
			DBG ( "Timer %p timeout updated to %ld\n",
			      timer, timer->timeout );
		}
	}
}

/**
 * Handle expired timer
 *
 * @v timer		Retry timer
 */
static void timer_expired ( struct retry_timer *timer ) {
	int fail;

	/* Stop timer without performing RTT calculations */
	DBG2 ( "Timer %p stopped at time %ld on expiry\n",
	       timer, currticks() );
	list_del ( &timer->list );
	timer->start = 0;
	timer->count++;

	/* Back off the timeout value */
	timer->timeout <<= 1;
	if ( ( fail = ( timer->timeout > MAX_TIMEOUT ) ) )
		timer->timeout = MAX_TIMEOUT;
	DBG ( "Timer %p timeout backed off to %ld\n",
	      timer, timer->timeout );

	/* Call expiry callback */
	timer->expired ( timer, fail );	
}

/**
 * Single-step the retry timer list
 *
 * @v process		Retry timer process
 */
static void retry_step ( struct process *process __unused ) {
	struct retry_timer *timer;
	struct retry_timer *tmp;
	unsigned long now = currticks();
	unsigned long used;

	list_for_each_entry_safe ( timer, tmp, &timers, list ) {
		used = ( now - timer->start );
		if ( used >= timer->timeout )
			timer_expired ( timer );
	}
}

/** Retry timer process */
struct process retry_process __permanent_process = {
	.step = retry_step,
};
