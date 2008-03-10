/*
 * Etherboot routines for PCBIOS firmware.
 *
 * Body of routines taken from old pcbios.S
 */

#include <gpxe/init.h>
#include <gpxe/timer.h>
#include <stdio.h>
#include <realmode.h>
#include <bios.h>
#include <bits/timer2.h>

/* A bit faster actually, but we don't care. */
#define	TIMER2_TICKS_PER_SEC	18

/*
 * Use direct memory access to BIOS variables, longword 0040:006C (ticks
 * today) and byte 0040:0070 (midnight crossover flag) instead of calling
 * timeofday BIOS interrupt.
 */

static tick_t bios_currticks ( void ) {
	static int days = 0;
	uint32_t ticks;
	uint8_t midnight;

	/* Re-enable interrupts so that the timer interrupt can occur */
	__asm__ __volatile__ ( REAL_CODE ( "sti\n\t"
					   "nop\n\t"
					   "nop\n\t"
					   "cli\n\t" ) : : );

	get_real ( ticks, BDA_SEG, 0x006c );
	get_real ( midnight, BDA_SEG, 0x0070 );

	if ( midnight ) {
		midnight = 0;
		put_real ( midnight, BDA_SEG, 0x0070 );
		days += 0x1800b0;
	}

	return ( (days + ticks) * (USECS_IN_SEC / TIMER2_TICKS_PER_SEC) );
}

static int bios_ts_init(void)
{
	DBG("BIOS timer installed\n");
	return 0;
}

struct timer bios_ts __timer ( 02 ) = {
	.init = bios_ts_init,
	.udelay = i386_timer2_udelay,
	.currticks = bios_currticks,
};

