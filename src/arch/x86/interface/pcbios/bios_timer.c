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
 * BIOS timer
 *
 */

#include <ipxe/timer.h>
#include <realmode.h>
#include <bios.h>
#include <ipxe/pit8254.h>

/** Number of ticks per day
 *
 * This seems to be the normative value, as used by e.g. SeaBIOS to
 * decide when to set the midnight rollover flag.
 */
#define BIOS_TICKS_PER_DAY 0x1800b0

/** Number of ticks per BIOS tick */
#define TICKS_PER_BIOS_TICK \
	( ( TICKS_PER_SEC * 60 * 60 * 24 ) / BIOS_TICKS_PER_DAY )

/**
 * Get current system time in ticks
 *
 * @ret ticks		Current time, in ticks
 *
 * Use direct memory access to BIOS variables, longword 0040:006C
 * (ticks today) and byte 0040:0070 (midnight crossover flag) instead
 * of calling timeofday BIOS interrupt.
 */
static unsigned long bios_currticks ( void ) {
	static uint32_t offset;
	uint32_t ticks;
	uint8_t midnight;

	/* Re-enable interrupts so that the timer interrupt can occur */
	__asm__ __volatile__ ( "sti\n\t"
			       "nop\n\t"
			       "nop\n\t"
			       "cli\n\t" );

	/* Read current BIOS time of day */
	get_real ( ticks, BDA_SEG, BDA_TICKS );
	get_real ( midnight, BDA_SEG, BDA_MIDNIGHT );

	/* Handle midnight rollover */
	if ( midnight ) {
		midnight = 0;
		put_real ( midnight, BDA_SEG, BDA_MIDNIGHT );
		offset += BIOS_TICKS_PER_DAY;
	}
	ticks += offset;

	/* Convert to timer ticks */
	return ( ticks * TICKS_PER_BIOS_TICK );
}

/** BIOS timer */
struct timer bios_timer __timer ( TIMER_NORMAL ) = {
	.name = "bios",
	.currticks = bios_currticks,
	.udelay = pit8254_udelay,
};
