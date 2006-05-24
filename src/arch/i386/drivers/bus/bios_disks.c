#include "realmode.h"
#include "console.h"
#include "disk.h"
#include "bios_disks.h"

#warning "This file is obsolete"
#if 0

#define CF ( 1 << 0 )
#define BIOS_DISK_NONE 0

/*
 * Reset the disk system using INT 13,0.  Forces both hard disks and
 * floppy disks to seek back to track 0.
 *
 */
static void bios_disk_init ( void ) {
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
 * Automatically retries up to three times to allow time for floppy
 * disks to spin up, calling bios_disk_init() after each failure.
 *
 */
static unsigned int bios_disk_read ( struct bios_disk_device *bios_disk,
				     unsigned int cylinder,
				     unsigned int head,
				     unsigned int sector,
				     struct bios_disk_sector *buf ) {
	uint16_t basemem_buf, ax, flags;
	unsigned int status, discard_c, discard_d;
	int retry = 3;

	basemem_buf = BASEMEM_PARAMETER_INIT ( *buf );
	do {
		REAL_EXEC ( rm_bios_disk_read,
			    "sti\n\t"
			    "movw $0x0201, %%ax\n\t" /* Read a single sector */
			    "int $0x13\n\t"
			    "pushfw\n\t"
			    "popw %%bx\n\t"
			    "cli\n\t",
			    4,
			    OUT_CONSTRAINTS ( "=a" ( ax ), "=b" ( flags ),
					      "=c" ( discard_c ),
					      "=d" ( discard_d ) ),
			    IN_CONSTRAINTS ( "c" ( ( (cylinder & 0xff) << 8 ) |
						   ( (cylinder >> 8) & 0x3 ) |
						   sector ),
					     "d" ( ( head << 8 ) |
						   bios_disk->drive ),
					     "b" ( basemem_buf ) ),
			    CLOBBER ( "ebp", "esi", "edi" ) );
		status = ( flags & CF ) ? ( ax >> 8 ) : 0;
	} while ( ( status != 0 ) && ( bios_disk_init(), retry-- ) );
	BASEMEM_PARAMETER_DONE ( *buf );

	return status;
}

/*
 * Increment a bus_loc structure to the next possible BIOS disk
 * location.  Leave the structure zeroed and return 0 if there are no
 * more valid locations.
 *
 */
static int bios_disk_next_location ( struct bus_loc *bus_loc ) {
	struct bios_disk_loc *bios_disk_loc
		= ( struct bios_disk_loc * ) bus_loc;
	
	/*
	 * Ensure that there is sufficient space in the shared bus
	 * structures for a struct bios_disk_loc and a struct
	 * bios_disk_dev, as mandated by bus.h.
	 *
	 */
	BUS_LOC_CHECK ( struct bios_disk_loc );
	BUS_DEV_CHECK ( struct bios_disk_device );

	return ( ++bios_disk_loc->drive );
}

/*
 * Fill in parameters for a BIOS disk device based on drive number
 *
 */
static int bios_disk_fill_device ( struct bus_dev *bus_dev,
				   struct bus_loc *bus_loc ) {
	struct bios_disk_loc *bios_disk_loc
		= ( struct bios_disk_loc * ) bus_loc;
	struct bios_disk_device *bios_disk
		= ( struct bios_disk_device * ) bus_dev;
	uint16_t flags;

	/* Store drive in struct bios_disk_device */
	bios_disk->drive = bios_disk_loc->drive;
       
	REAL_EXEC ( rm_bios_disk_exists,
		    "sti\n\t"
		    "movb $0x15, %%ah\n\t"
		    "int $0x13\n\t"
		    "pushfw\n\t"
		    "popw %%dx\n\t"
		    "movb %%ah, %%al\n\t"
		    "cli\n\t",
		    2,
		    OUT_CONSTRAINTS ( "=a" ( bios_disk->type ),
				      "=d" ( flags ) ),
		    IN_CONSTRAINTS ( "d" ( bios_disk->drive ) ),
		    CLOBBER ( "ebx", "ecx", "esi", "edi", "ebp" ) );

	if ( ( flags & CF ) || ( bios_disk->type == BIOS_DISK_NONE ) )
		return 0;

	DBG ( "BIOS disk found valid drive %hhx\n", bios_disk->drive );
	return 1;
}

/*
 * Test whether or not a driver is capable of driving the device.
 *
 */
static int bios_disk_check_driver ( struct bus_dev *bus_dev,
				    struct device_driver *device_driver ) {
	struct bios_disk_device *bios_disk
		= ( struct bios_disk_device * ) bus_dev;
	struct bios_disk_driver *driver
		= ( struct bios_disk_driver * ) device_driver->bus_driver_info;

	/* Compare against driver's valid ID range */
	if ( ( bios_disk->drive >= driver->min_drive ) &&
	     ( bios_disk->drive <= driver->max_drive ) ) {
		driver->fill_drive_name ( bios_disk->name, bios_disk->drive );
		DBG ( "BIOS disk found drive %hhx (\"%s\") "
		      "matching driver %s\n",
		      bios_disk->drive, bios_disk->name,
		      driver->name );
		return 1;
	}

	return 0;
}

/*
 * Describe a BIOS disk device
 *
 */
static char * bios_disk_describe_device ( struct bus_dev *bus_dev ) {
	struct bios_disk_device *bios_disk
		= ( struct bios_disk_device * ) bus_dev;
	static char bios_disk_description[] = "BIOS disk 00";

	sprintf ( bios_disk_description + 10, "%hhx", bios_disk->drive );
	return bios_disk_description;
}

/*
 * Name a BIOS disk device
 *
 */
static const char * bios_disk_name_device ( struct bus_dev *bus_dev ) {
	struct bios_disk_device *bios_disk
		= ( struct bios_disk_device * ) bus_dev;
	
	return bios_disk->name;
}

/*
 * BIOS disk bus operations table
 *
 */
struct bus_driver bios_disk_driver __bus_driver = {
	.name			= "BIOS DISK",
	.next_location		= bios_disk_next_location,
	.fill_device		= bios_disk_fill_device,
	.check_driver		= bios_disk_check_driver,
	.describe_device	= bios_disk_describe_device,
	.name_device		= bios_disk_name_device,
};

/*
 * Fill in a disk structure
 *
 */
void bios_disk_fill_disk ( struct disk *disk __unused,
			   struct bios_disk_device *bios_disk __unused ) {

}

#endif
