/*
 * Copyright (C) 2010 Piotr Jaroszyński <p.jaroszynski@gmail.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

FILE_LICENCE(GPL2_OR_LATER);

#include <stddef.h>
#include <ipxe/timer.h>

#include <linux_api.h>

/** @file
 *
 * iPXE timer API for linux
 *
 */

/**
 * Delay for a fixed number of microseconds
 *
 * @v usecs		Number of microseconds for which to delay
 */
static void linux_udelay(unsigned long usecs)
{
	linux_usleep(usecs);
}

/**
 * Get current system time in ticks
 *
 * linux doesn't provide an easy access to jiffies so implement it by measuring
 * the time since the first call to this function.
 *
 * Since this function is used to seed the (non-cryptographic) random
 * number generator, we round the start time down to the nearest whole
 * second.  This minimises the chances of generating identical RNG
 * sequences (and hence identical TCP port numbers, etc) on
 * consecutive invocations of iPXE.
 *
 * @ret ticks		Current time, in ticks
 */
static unsigned long linux_currticks(void)
{
	static struct timeval start;
	static int initialized = 0;
	struct timeval now;
	unsigned long ticks;

	if (! initialized) {
		linux_gettimeofday(&start, NULL);
		initialized = 1;
	}

	linux_gettimeofday(&now, NULL);

	ticks = ( ( now.tv_sec - start.tv_sec ) * TICKS_PER_SEC );
	ticks += ( now.tv_usec / ( 1000000 / TICKS_PER_SEC ) );

	return ticks;
}

/** Linux timer */
struct timer linux_timer __timer ( TIMER_NORMAL ) = {
	.name = "linux",
	.currticks = linux_currticks,
	.udelay = linux_udelay,
};
