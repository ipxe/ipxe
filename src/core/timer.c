/*
 * core/timer.c
 *
 * Copyright (C) 2007 Alexey Zaytsev <alexey.zaytsev@gmail.com>
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
#include <assert.h>
#include <gpxe/init.h>
#include <gpxe/timer.h>

static struct timer ts_table[0]
	__table_start ( struct timer, timers );
static struct timer ts_table_end[0]
	__table_end ( struct timer, timers );


static struct timer *used_ts = NULL;

/*
 * This function may be used in custom timer driver.
 *
 * This udelay implementation works well if you've got a
 * fast currticks().
 */
void generic_currticks_udelay(unsigned int usecs)
{
	tick_t t;

	t = currticks();
	while (t + usecs > currticks())
		; /* xxx: Relax the cpu some way. */
}


static void timer_init(void)
{
	struct timer *ts;

	for (ts = ts_table; ts < ts_table_end; ts++) {
		if ( ts->init() == 0 ) {
			used_ts = ts;
			return;
		}
	}

	/* No timer found; we cannot continue */
	assert ( 0 );
	while ( 1 ) {};
}

struct init_fn ts_init_fn __init_fn ( INIT_NORMAL ) = {
	.initialise = timer_init,
};

/**
 * Read current time
 *
 * @ret ticks	Current time, in ticks
 */
tick_t currticks ( void ) {
	tick_t ct;
	assert(used_ts);

	ct = used_ts->currticks();
	DBG ( "currticks: %ld.%06ld seconds\n",
	      ct / USECS_IN_SEC, ct % USECS_IN_SEC );

	return ct;
}

/**
 * Delay
 *
 * @v usecs	Time to delay, in microseconds
 */
void udelay ( unsigned int usecs ) {
	assert(used_ts);
	used_ts->udelay ( usecs );
}

/**
 * Delay
 *
 * @v msecs	Time to delay, in milliseconds
 */
void mdelay ( unsigned int msecs ) {
	while ( msecs-- )
		udelay ( USECS_IN_MSEC );
}

/**
 * Delay
 *
 * @v secs	Time to delay, in seconds
 */
unsigned int sleep ( unsigned int secs ) {
	while ( secs-- )
		mdelay ( MSECS_IN_SEC );
	return 0;
}
