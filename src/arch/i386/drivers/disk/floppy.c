#include "console.h"
#include "disk.h"
#include "bios_disks.h"

static void fill_floppy_name ( char *buf, uint8_t drive ) {
	sprintf ( buf, "fd%d", drive );
}

static struct disk_operations floppy_operations = {

};

static int floppy_probe ( struct disk *disk,
			  struct bios_disk_device *bios_disk ) {
	
	return 1;
}

static void floppy_disable ( struct disk *disk,
			     struct bios_disk_device *bios_disk ) {
	
}

static struct bios_disk_driver floppy_driver =
	BIOS_DISK_DRIVER ( fill_floppy_name, 0x00, 0x7f );

DRIVER ( "floppy", disk_driver, bios_disk_driver, floppy_driver,
	 floppy_probe, floppy_disable );
