/*
 * Copyright (C) 2018 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <ipxe/io.h>
#include <ipxe/acpi.h>

/** @file
 *
 * ACPI power management timer
 *
 */

/** ACPI timer frequency (fixed 3.579545MHz) */
#define ACPI_TIMER_HZ 3579545

/** ACPI timer mask
 *
 * Timers may be implemented as either 24-bit or 32-bit counters.  We
 * simplify the code by pessimistically assuming that the timer has
 * only 24 bits.
 */
#define ACPI_TIMER_MASK 0x00ffffffUL

/** Power management timer register address */
static unsigned int pm_tmr;

struct timer acpi_timer __timer ( TIMER_PREFERRED );

/**
 * Get current system time in ticks
 *
 * @ret ticks		Current time, in ticks
 */
static unsigned long acpi_currticks ( void ) {
	static unsigned long offset;
	static uint32_t prev;
	uint32_t now;

	/* Read timer and account for wraparound */
	now = ( inl ( pm_tmr ) & ACPI_TIMER_MASK );
	if ( now < prev ) {
		offset += ( ( ACPI_TIMER_MASK + 1 ) /
			    ( ACPI_TIMER_HZ / TICKS_PER_SEC ) );
	}
	prev = now;

	/* Convert to timer ticks */
	return ( offset + ( now / ( ACPI_TIMER_HZ / TICKS_PER_SEC ) ) );
}

/**
 * Delay for a fixed number of microseconds
 *
 * @v usecs		Number of microseconds for which to delay
 */
static void acpi_udelay ( unsigned long usecs ) {
	uint32_t start;
	uint32_t elapsed;
	uint32_t threshold;

	/* Delay until a suitable number of ticks have elapsed.  We do
	 * not need to allow for multiple wraparound, since the
	 * wraparound period for a 24-bit timer at 3.579545MHz is
	 * around 4700000us.
	 */
	start = inl ( pm_tmr );
	threshold = ( ( usecs * ACPI_TIMER_HZ ) / 1000000 );
	do {
		elapsed = ( ( inl ( pm_tmr ) - start ) & ACPI_TIMER_MASK );
	} while ( elapsed < threshold );
}

/**
 * Probe ACPI power management timer
 *
 * @ret rc		Return status code
 */
static int acpi_timer_probe ( void ) {
	const struct acpi_fadt *fadt;
	unsigned int pm_tmr_blk;

	/* Locate FADT */
	fadt = container_of ( acpi_table ( FADT_SIGNATURE, 0 ),
			      struct acpi_fadt, acpi );
	if ( ! fadt ) {
		DBGC ( &acpi_timer, "ACPI could not find FADT\n" );
		return -ENOENT;
	}

	/* Read FADT */
	pm_tmr_blk = le32_to_cpu ( fadt->pm_tmr_blk );
	if ( ! pm_tmr_blk ) {
		DBGC ( &acpi_timer, "ACPI has no timer\n" );
		return -ENOENT;
	}

	/* Record power management timer register address */
	pm_tmr = ( pm_tmr_blk + ACPI_PM_TMR );

	return 0;
}

/** ACPI timer */
struct timer acpi_timer __timer ( TIMER_PREFERRED ) = {
	.name = "acpi",
	.probe = acpi_timer_probe,
	.currticks = acpi_currticks,
	.udelay = acpi_udelay,
};
