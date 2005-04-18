/* Etherboot routines for PCBIOS firmware.
 *
 * Body of routines taken from old pcbios.S
 */

#include "stdint.h"
#include "realmode.h"

#define BIOS_DATA_SEG 0x0040

#define CF ( 1 << 0 )

/**************************************************************************
CURRTICKS - Get Time
Use direct memory access to BIOS variables, longword 0040:006C (ticks
today) and byte 0040:0070 (midnight crossover flag) instead of calling
timeofday BIOS interrupt.
**************************************************************************/
#if defined(CONFIG_TSC_CURRTICKS)
#undef CONFIG_BIOS_CURRTICKS
#else
#define CONFIG_BIOS_CURRTICKS 1
#endif
#if defined(CONFIG_BIOS_CURRTICKS)
unsigned long currticks ( void ) {
	static uint32_t days = 0;
	uint32_t ticks;
	uint8_t midnight;

	/* Re-enable interrupts so that the timer interrupt can occur
	 */
	REAL_EXEC ( rm_currticks,
		    "sti\n\t"
		    "nop\n\t"
		    "nop\n\t"
		    "cli\n\t",
		    0,
		    OUT_CONSTRAINTS (),
		    IN_CONSTRAINTS (),
		    CLOBBER ( "eax" ) ); /* can't have an empty clobber list */

	get_real ( ticks, BIOS_DATA_SEG, 0x006c );
	get_real ( midnight, BIOS_DATA_SEG, 0x0070 );

	if ( midnight ) {
		midnight = 0;
		put_real ( midnight, BIOS_DATA_SEG, 0x0070 );
		days += 0x1800b0;
	}
	return ( days + ticks );
}
#endif	/* CONFIG_BIOS_CURRTICKS */

/**************************************************************************
CPU_NAP - Save power by halting the CPU until the next interrupt
**************************************************************************/
void cpu_nap ( void ) {
	REAL_EXEC ( rm_cpu_nap,
		    "sti\n\t"
		    "hlt\n\t"
		    "cli\n\t",
		    0,
		    OUT_CONSTRAINTS (),
		    IN_CONSTRAINTS (),
		    CLOBBER ( "eax" ) ); /* can't have an empty clobber list */
}
