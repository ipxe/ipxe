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

#if	(TRY_FLOPPY_FIRST > 0)
/**************************************************************************
DISK_INIT - Initialize the disk system
**************************************************************************/
void disk_init ( void ) {
	REAL_EXEC ( rm_disk_init,
		    "sti\n\t"
		    "xorw %ax,%ax\n\t"
		    "movb $0x80,%dl\n\t"
		    "int $0x13\n\t"
		    "cli\n\t",
		    0,
		    OUT_CONSTRAINTS (),
		    IN_CONSTRAINTS (),
   		    CLOBBER ( "eax", "ebx", "ecx", "edx",
			      "ebp", "esi", "edi" ) );
}

/**************************************************************************
DISK_READ - Read a sector from disk
**************************************************************************/
unsigned int pcbios_disk_read ( int drive, int cylinder, int head, int sector,
				char *fixme_buf ) {
	uint16_t ax, flags, discard_c, discard_d;
	segoff_t buf = SEGOFF ( fixme_buf );

	/* FIXME: buf should be passed in as a segoff_t rather than a
	 * char *
	 */

	REAL_EXEC ( rm_pcbios_disk_read,
		    "sti\n\t"
		    "pushl %%ebx\n\t"	   /* Convert %ebx to %es:bx */
		    "popl %%bx\n\t"
		    "popl %%es\n\t"
		    "movb $0x02, %%ah\n\t" /* INT 13,2 - Read disk sector */
		    "movb $0x01, %%al\n\t" /* Read one sector */
		    "int $0x13\n\t"
		    "pushfw\n\t"
		    "popw %%bx\n\t"
		    "cli\n\t",
		    4,
		    OUT_CONSTRAINTS ( "=a" ( ax ), "=b" ( flags ),
				      "=c" ( discard_c ), "=d" ( discard_d ) ),
		    IN_CONSTRAINTS ( "c" ( ( ( cylinder & 0xff ) << 8 ) |
					   ( ( cylinder >> 8 ) & 0x3 ) |
					   sector ),
				     "d" ( ( head << 8 ) | drive ),
				     "b" ( buf ) ),
		    CLOBBER ( "ebp", "esi", "edi" ) );
	);

	return ( flags & CF ) ? ax : 0;
}
#endif /* TRY_FLOPPY_FIRST */
