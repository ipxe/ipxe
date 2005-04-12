#include "etherboot.h"
#include "stddef.h"
#include "dev.h"

/* Defined by linker */
extern struct boot_driver boot_drivers[];
extern struct boot_driver boot_drivers_end[];

/* Current attempted boot driver */
static struct boot_driver *boot_driver = boot_drivers;

/* Current boot device */
struct dev dev;

/* Print all drivers */
void print_drivers ( void ) {
	struct boot_driver *driver;

	for ( driver = boot_drivers ; driver < boot_drivers_end ; driver++ ) {
		printf ( "%s ", driver->name );
	}
}

/* Get the next available boot device */
int probe ( struct dev *dev ) {
	
	for ( ; boot_driver < boot_drivers_end ; boot_driver++ ) {
		dev->name = "unknown";
		if ( boot_driver->probe ( dev ) )
			return 1;
	}
	
	/* No more boot devices found */
	boot_driver = boot_drivers;
	return 0;
}

/* Disable a device */
void disable ( struct dev *dev ) {
	if ( dev->dev_op ) {
		dev->dev_op->disable ( dev );
		dev->dev_op = NULL;
	}
}
