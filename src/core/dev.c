#include "etherboot.h"
#include "stddef.h"
#include "dev.h"

/*
 * Each driver specifies a name, the bus-scanning function
 * (find_bus_boot_device) that it wants to use, a driver information
 * structure (bus_driver) containing e.g. device IDs to be passed to
 * find_bus_boot_device, and a probe function (probe) to be called
 * whenever a suitable device is found.
 *
 * The generic device-probing code knows nothing about particular bus
 * types; it simply passes the driver information structure
 * (bus_driver) to the bus-scanning function (find_bus_boot_device),
 * then passes the result of that function (if not NULL) to the probe
 * function (probe).
 */

/* Defined by linker */
extern struct boot_driver boot_drivers[];
extern struct boot_driver boot_drivers_end[];

/* Current attempted boot driver */
static struct boot_driver *boot_driver = boot_drivers;

/* Print all drivers */
void print_drivers ( void ) {
	struct boot_driver *driver;

	for ( driver = boot_drivers ; driver < boot_drivers_end ; driver++ ) {
		printf ( "%s ", driver->name );
	}
}

/* Get the next available boot device */
int find_boot_device ( struct dev *dev ) {
	for ( ; boot_driver < boot_drivers_end ; boot_driver++ ) {
		dev->driver = boot_driver;
		dev->name = boot_driver->name;
		DBG ( "Probing driver %s...\n", dev->name );
		if (  boot_driver->find_bus_boot_device ( dev,
						  boot_driver->bus_driver ) ) {
			DBG ( "Found device %s (ID %hhx:%hx:%hx)\n",
			      dev->name, dev->devid->bus_type,
			      dev->devid->vendor_id, dev->devid->device_id );
			return 1;
		}
	}

	/* No more boot devices found */
	boot_driver = boot_drivers;
	return 0;
}

/* Probe the boot device */
int probe ( struct dev *dev ) {
	return dev->driver->probe ( dev, dev->bus );
}

/* Disable a device */
void disable ( struct dev *dev ) {
	if ( dev->dev_op ) {
		dev->dev_op->disable ( dev );
		dev->dev_op = NULL;
	}
}
