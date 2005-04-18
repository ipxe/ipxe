#include "realmode.h"

#define CF ( 1 << 0 )

/**************************************************************************
DISK_INIT - Initialize the disk system
**************************************************************************/
void disk_init ( void ) {
	REAL_EXEC ( rm_disk_init,
		    "sti\n\t"
		    "xorw %%ax,%%ax\n\t"
		    "movb $0x80,%%dl\n\t"
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
		    "popw %%bx\n\t"
		    "popw %%es\n\t"
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

	return ( flags & CF ) ? ax : 0;
}
