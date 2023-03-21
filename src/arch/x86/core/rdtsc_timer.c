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

/** @file
 *
 * RDTSC timer
 *
 */

#include <string.h>
#include <errno.h>
#include <ipxe/timer.h>
#include <ipxe/cpuid.h>
#include <ipxe/pit8254.h>

/** Number of microseconds to use for TSC calibration */
#define TSC_CALIBRATE_US 1024

/** TSC increment per microsecond */
static unsigned long tsc_per_us;

/** Minimum resolution for scaled TSC timer */
#define TSC_SCALED_HZ 32

/** TSC scale (expressed as a bit shift)
 *
 * We use this to avoid the need for 64-bit divsion on 32-bit systems.
 */
static unsigned int tsc_scale;

/** Number of timer ticks per scaled TSC increment */
static unsigned long ticks_per_scaled_tsc;

/** Colour for debug messages */
#define colour &tsc_per_us

/**
 * Get raw TSC value
 *
 * @ret tsc		Raw TSC value
 */
static inline __always_inline unsigned long rdtsc_raw ( void ) {
	unsigned long raw;

	__asm__ __volatile__ ( "rdtsc\n\t" : "=a" ( raw ) : : "edx" );
	return raw;
}

/**
 * Get TSC value, shifted to avoid rollover within a realistic timescale
 *
 * @ret tsc		Scaled TSC value
 */
static inline __always_inline unsigned long rdtsc_scaled ( void ) {
	unsigned long scaled;

	__asm__ __volatile__ ( "rdtsc\n\t"
			       "shrdl %b1, %%edx, %%eax\n\t"
			       : "=a" ( scaled ) : "c" ( tsc_scale ) : "edx" );
	return scaled;
}

/**
 * Get current system time in ticks
 *
 * @ret ticks		Current time, in ticks
 */
static unsigned long rdtsc_currticks ( void ) {
	unsigned long scaled;

	scaled = rdtsc_scaled();
	return ( scaled * ticks_per_scaled_tsc );
}

/**
 * Delay for a fixed number of microseconds
 *
 * @v usecs		Number of microseconds for which to delay
 */
static void rdtsc_udelay ( unsigned long usecs ) {
	unsigned long start;
	unsigned long elapsed;
	unsigned long threshold;

	start = rdtsc_raw();
	threshold = ( usecs * tsc_per_us );
	do {
		elapsed = ( rdtsc_raw() - start );
	} while ( elapsed < threshold );
}

/**
 * Probe RDTSC timer
 *
 * @ret rc		Return status code
 */
static int rdtsc_probe ( void ) {
	unsigned long before;
	unsigned long after;
	unsigned long elapsed;
	uint32_t apm;
	uint32_t discard_a;
	uint32_t discard_b;
	uint32_t discard_c;
	int rc;

	/* Check that TSC is invariant */
	if ( ( rc = cpuid_supported ( CPUID_APM ) ) != 0 ) {
		DBGC ( colour, "RDTSC cannot determine APM features: %s\n",
		       strerror ( rc ) );
		return rc;
	}
	cpuid ( CPUID_APM, 0, &discard_a, &discard_b, &discard_c, &apm );
	if ( ! ( apm & CPUID_APM_EDX_TSC_INVARIANT ) ) {
		DBGC ( colour, "RDTSC has non-invariant TSC (%#08x)\n",
		       apm );
		return -ENOTTY;
	}

	/* Calibrate udelay() timer via 8254 PIT */
	before = rdtsc_raw();
	pit8254_udelay ( TSC_CALIBRATE_US );
	after = rdtsc_raw();
	elapsed = ( after - before );
	tsc_per_us = ( elapsed / TSC_CALIBRATE_US );
	if ( ! tsc_per_us ) {
		DBGC ( colour, "RDTSC has zero TSC per microsecond\n" );
		return -EIO;
	}

	/* Calibrate currticks() scaling factor */
	tsc_scale = 31;
	ticks_per_scaled_tsc = ( ( 1UL << tsc_scale ) /
				 ( tsc_per_us * ( 1000000 / TICKS_PER_SEC ) ) );
	while ( ticks_per_scaled_tsc > ( TICKS_PER_SEC / TSC_SCALED_HZ ) ) {
		tsc_scale--;
		ticks_per_scaled_tsc >>= 1;
	}
	DBGC ( colour, "RDTSC has %ld tsc per us, %ld ticks per 2^%d tsc\n",
	       tsc_per_us, ticks_per_scaled_tsc, tsc_scale );
	if ( ! ticks_per_scaled_tsc ) {
		DBGC ( colour, "RDTSC has zero ticks per TSC\n" );
		return -EIO;
	}

	return 0;
}

/** RDTSC timer */
struct timer rdtsc_timer __timer ( TIMER_PREFERRED ) = {
	.name = "rdtsc",
	.probe = rdtsc_probe,
	.currticks = rdtsc_currticks,
	.udelay = rdtsc_udelay,
};
