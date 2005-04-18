#include "realmode.h"

#define CF ( 1 << 0 )

struct disk_sector {
	char data[512];
};

/*
 * Reset the disk system using INT 13,0.  Forces both hard disks and
 * floppy disks to seek back to track 0.
 *
 */
static void disk_init ( void ) {
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

/*
 * Read a single sector from a disk using INT 13,2.
 *
 * Returns the BIOS status code (%ah) - 0 indicates success
 *
 */
static unsigned int pcbios_disk_read ( int drive, int cylinder, int head,
				       int sector, struct disk_sector *buf ) {
	uint16_t basemem_buf, status, flags;
	int discard_c, discard_d;

	basemem_buf = BASEMEM_PARAMETER_INIT ( *buf );
	REAL_EXEC ( rm_pcbios_disk_read,
		    "sti\n\t"
		    "movw $0x0201, %%ax\n\t" /* Read a single sector */
		    "int $0x13\n\t"
		    "pushfw\n\t"
		    "popw %%bx\n\t"
		    "cli\n\t",
		    4,
		    OUT_CONSTRAINTS ( "=a" ( status ), "=b" ( flags ),
				      "=c" ( discard_c ), "=d" ( discard_d ) ),
		    IN_CONSTRAINTS ( "c" ( ( ( cylinder & 0xff ) << 8 ) |
					   ( ( cylinder >> 8 ) & 0x3 ) |
					   sector ),
				     "d" ( ( head << 8 ) | drive ),
				     "b" ( basemem_buf ) ),
		    CLOBBER ( "ebp", "esi", "edi" ) );
	BASEMEM_PARAMETER_DONE ( *buf );

	return ( flags & CF ) ? ( status >> 8 ) : 0;
}
