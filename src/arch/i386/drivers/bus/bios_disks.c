#include "realmode.h"
#include "isa_ids.h"
#include "bios_disks.h"

#define CF ( 1 << 0 )
#define BIOS_DISK_NONE 0

/*
 * Ensure that there is sufficient space in the shared dev_bus
 * structure for a struct bios_disk_device.
 *
 */
DEV_BUS( struct bios_disk_device, bios_disk_dev );
static char bios_disk_magic[0]; /* guaranteed unique symbol */

/*
 * Reset the disk system using INT 13,0.  Forces both hard disks and
 * floppy disks to seek back to track 0.
 *
 */
void bios_disk_init ( void ) {
	REAL_EXEC ( rm_bios_disk_init,
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
 * Returns the BIOS status code (%ah) - 0 indicates success.
 *
 */
unsigned int bios_disk_read_once ( struct bios_disk_device *bios_disk,
				   unsigned int cylinder,
				   unsigned int head,
				   unsigned int sector,
				   struct bios_disk_sector *buf ) {
	uint16_t basemem_buf, status, flags;
	int discard_c, discard_d;

	basemem_buf = BASEMEM_PARAMETER_INIT ( *buf );
	REAL_EXEC ( rm_bios_disk_read,
		    "sti\n\t"
		    "movw $0x0201, %%ax\n\t" /* Read a single sector */
		    "int $0x13\n\t"
		    "pushfw\n\t"
		    "popw %%bx\n\t"
		    "cli\n\t",
		    4,
		    OUT_CONSTRAINTS ( "=a" ( status ), "=b" ( flags ),
				      "=c" ( discard_c ),
				      "=d" ( discard_d ) ),
		    IN_CONSTRAINTS ( "c" ( ( ( cylinder & 0xff ) << 8 ) |
					   ( ( cylinder >> 8 ) & 0x3 ) |
					   sector ),
				     "d" ( ( head << 8 ) | bios_disk->drive ),
				     "b" ( basemem_buf ) ),
		    CLOBBER ( "ebp", "esi", "edi" ) );
	BASEMEM_PARAMETER_DONE ( *buf );

	return ( flags & CF ) ? ( status >> 8 ) : 0; 
}

/*
 * Fill in parameters for a BIOS disk device based on drive number
 *
 */
static int fill_bios_disk_device ( struct bios_disk_device *bios_disk ) {
	uint16_t type, flags;
       
	REAL_EXEC ( rm_bios_disk_exists,
		    "sti\n\t"
		    "movb $0x15, %%ah\n\t"
		    "int $0x13\n\t"
		    "pushfw\n\t"
		    "popw %%dx\n\t"
		    "cli\n\t",
		    2,
		    OUT_CONSTRAINTS ( "=a" ( type ), "=d" ( flags ) ),
		    IN_CONSTRAINTS ( "d" ( bios_disk->drive ) ),
		    CLOBBER ( "ebx", "ecx", "esi", "edi", "ebp" ) );

	if ( ( flags & CF ) ||
	     ( ( type >> 8 ) == BIOS_DISK_NONE ) )
		return 0;

	DBG ( "BIOS disk found valid drive %hhx\n", bios_disk->drive );
	return 1;
}

/*
 * Find a BIOS disk device matching the specified driver
 *
 */
int find_bios_disk_device ( struct bios_disk_device *bios_disk,
			    struct bios_disk_driver *driver ) {

	/* Initialise struct bios_disk if it's the first time it's been used.
	 */
	if ( bios_disk->magic != bios_disk_magic ) {
		memset ( bios_disk, 0, sizeof ( *bios_disk ) );
		bios_disk->magic = bios_disk_magic;
	}

	/* Iterate through all possible BIOS drives, starting where we
	 * left off
	 */
	DBG ( "BIOS disk searching for device matching driver %s\n",
	      driver->name );
	do {
		/* If we've already used this device, skip it */
		if ( bios_disk->already_tried ) {
			bios_disk->already_tried = 0;
			continue;
		}

		/* Fill in device parameters */
		if ( ! fill_bios_disk_device ( bios_disk ) ) {
			continue;
		}

		/* Compare against driver's valid ID range */
		if ( ( bios_disk->drive >= driver->min_drive ) &&
		     ( bios_disk->drive <= driver->max_drive ) ) {
			driver->fill_drive_name ( bios_disk->drive,
						  bios_disk->name );
			DBG ( "BIOS_DISK found drive %hhx (\"%s\") "
			      "matching driver %s\n",
			      bios_disk->drive, bios_disk->name,
			      driver->name );
			bios_disk->already_tried = 1;
			return 1;
		}
	} while ( ++bios_disk->drive );

	/* No device found */
	DBG ( "BIOS disk found no device matching driver %s\n", driver->name );
	return 0;
}

/*
 * Find the next MCA device that can be used to boot using the
 * specified driver.
 *
 */
int find_bios_disk_boot_device ( struct dev *dev,
				 struct bios_disk_driver *driver ) {
	struct bios_disk_device *bios_disk
		= ( struct bios_disk_device * ) dev->bus;

	if ( ! find_bios_disk_device ( bios_disk, driver ) )
		return 0;

	dev->name = bios_disk->name;
	dev->devid.bus_type = ISA_BUS_TYPE;
	dev->devid.vendor_id = ISA_VENDOR ( 'D', 'S', 'K' );
	dev->devid.device_id = bios_disk->drive;

	return 1;
}
