/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Base counters and timers extension (Zicntr)
 *
 */

#include <string.h>
#include <errno.h>
#include <ipxe/fdt.h>
#include <ipxe/csr.h>
#include <ipxe/timer.h>

/** Timer increment per microsecond */
static unsigned long zicntr_mhz;

/** Minimum resolution for scaled timer */
#define ZICNTR_SCALED_HZ 32

/**
 * Timer scale (expressed as a bit shift)
 *
 * We use this to avoid the need for 64-bit divsion on 32-bit systems.
 */
static unsigned int zicntr_scale;

/** Number of timer ticks per scaled timer increment */
static unsigned long zicntr_ticks;

/** Colour for debug messages */
#define colour &zicntr_mhz

/**
 * Get low XLEN bits of current time
 *
 * @ret time		Current time
 */
static inline __attribute__ (( always_inline )) unsigned long
rdtime_low ( void ) {
	unsigned long time;

	/* Read low XLEN bits of current time */
	__asm__ __volatile__ ( "rdtime %0" : "=r" ( time ) );
	return time;
}

/**
 * Get current time, scaled to avoid rollover within a realistic timescale
 *
 * @ret time		Scaled current time
 */
static inline __attribute__ (( always_inline )) unsigned long
rdtime_scaled ( void ) {
	union {
		uint64_t time;
		struct {
			uint32_t low;
			uint32_t high;
		};
	} u;
	unsigned long tmp __attribute__ (( unused ));

	/* Read full current time */
#if __riscv_xlen >= 64
	__asm__ __volatile__ ( "rdtime %0" : "=r" ( u.time ) );
#else
	__asm__ __volatile__ ( "1:\n\t"
			       "rdtimeh %1\n\t"
			       "rdtime %0\n\t"
			       "rdtimeh %2\n\t"
			       "bne %1, %2, 1b\n\t"
			       : "=r" ( u.low ), "=r" ( u.high ),
				 "=r" ( tmp ) );
#endif

	/* Scale time to avoid XLEN-bit rollover */
	return ( u.time >> zicntr_scale );
}

/**
 * Get current system time in ticks
 *
 * @ret ticks		Current time, in ticks
 */
static unsigned long zicntr_currticks ( void ) {
	unsigned long scaled;

	/* Get scaled time and convert to ticks */
	scaled = rdtime_scaled();
	return ( scaled * zicntr_ticks );
}

/**
 * Delay for a fixed number of microseconds
 *
 * @v usecs		Number of microseconds for which to delay
 */
static void zicntr_udelay ( unsigned long usecs ) {
	unsigned long start;
	unsigned long elapsed;
	unsigned long threshold;

	/* Delay until sufficient time has elapsed */
	start = rdtime_low();
	threshold = ( usecs * zicntr_mhz );
	do {
		elapsed = ( rdtime_low() - start );
	} while ( elapsed < threshold );
}

/**
 * Probe timer
 *
 * @ret rc		Return status code
 */
static int zicntr_probe ( void ) {
	unsigned int offset;
	union {
		uint64_t freq;
		int64_t sfreq;
	} u;
	int rc;

	/* Check if time CSR can be read */
	if ( ! csr_can_read ( "time" ) ) {
		DBGC ( colour, "ZICNTR cannot read TIME CSR\n" );
		return -ENOTSUP;
	}

	/* Get timer frequency */
	if ( ( ( rc = fdt_path ( &sysfdt, "/cpus", &offset ) ) != 0 ) ||
	     ( ( rc = fdt_u64 ( &sysfdt, offset, "timebase-frequency",
				&u.freq ) ) != 0 ) ) {
		DBGC ( colour, "ZICNTR could not determine frequency: %s\n",
		       strerror ( rc ) );
		return rc;
	}

	/* Convert to MHz (without 64-bit division) */
	do {
		zicntr_mhz++;
		u.sfreq -= 1000000;
	} while ( u.sfreq > 0 );

	/* Calibrate currticks() scaling factor */
	zicntr_scale = 31;
	zicntr_ticks = ( ( 1UL << zicntr_scale ) /
			 ( zicntr_mhz * ( 1000000 / TICKS_PER_SEC ) ) );
	while ( zicntr_ticks > ( TICKS_PER_SEC / ZICNTR_SCALED_HZ ) ) {
		zicntr_scale--;
		zicntr_ticks >>= 1;
	}
	DBGC ( colour, "ZICNTR at %ld MHz, %ld ticks per 2^%d increments\n",
	       zicntr_mhz, zicntr_ticks, zicntr_scale );
	if ( ! zicntr_ticks ) {
		DBGC ( colour, "ZICNTR has zero ticks per 2^%d increments\n",
		       zicntr_scale );
		return -EIO;
	}

	return 0;
}

/** Zicntr timer */
struct timer zicntr_timer __timer ( TIMER_PREFERRED ) = {
	.name = "zicntr",
	.probe = zicntr_probe,
	.currticks = zicntr_currticks,
	.udelay = zicntr_udelay,
};
