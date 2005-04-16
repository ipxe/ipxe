#include "etherboot.h"
#include "isa.h"

/*
 * isa.c implements a "classical" port-scanning method of ISA device
 * detection.  The driver must provide a list of probe addresses
 * (probe_addrs), together with a function (probe_addr) that can be
 * used to test for the physical presence of a device at any given
 * address.
 *
 * Note that this should probably be considered the "last resort" for
 * device probing.  If the card supports ISAPnP or EISA, use that
 * instead.  Some cards (e.g. the 3c509) implement a proprietary
 * ISAPnP-like mechanism.
 *
 * The ISA probe address list can be overridden by config.c; if the
 * user specifies ISA_PROBE_ADDRS then that list will be used first.
 * (If ISA_PROBE_ADDRS ends with a zero, the driver's own list will
 * never be used).
 */

/*
 * Ensure that there is sufficient space in the shared dev_bus
 * structure for a struct isa_device.
 *
 */
DEV_BUS( struct isa_device, isa_dev );
static char isa_magic[0]; /* guaranteed unique symbol */

/*
 * Find an ISA device matching the specified driver
 *
 */
int find_isa_device ( struct isa_device *isa, struct isa_driver *driver ) {
	unsigned int i;
	uint16_t ioaddr;

	/* Initialise struct isa if it's the first time it's been used. */
	if ( isa->magic != isa_magic ) {
		memset ( isa, 0, sizeof ( *isa ) );
		isa->magic = isa_magic;
	}

	/* Iterate through any ISA probe addresses specified by
	 * config.c, starting where we left off.
	 */
	for ( i = isa->probe_idx ; i < isa_extra_probe_addr_count ; i++ ) {
		/* If we've already used this device, skip it */
		if ( isa->already_tried ) {
			isa->already_tried = 0;
			continue;
		}

		/* Set I/O address */
		ioaddr = isa_extra_probe_addrs[i];

		/* An I/O address of 0 in extra_probe_addrs list means
		 * stop probing (i.e. don't continue to the
		 * driver-provided list)
		 */
		if ( ! ioaddr )
			goto notfound;

		/* Use probe_addr method to see if there's a device
		 * present at this address.
		 */
		if ( driver->probe_addr ( ioaddr ) ) {
			isa->probe_idx = i;
			goto found;
		}
	}

	/* Iterate through all ISA probe addresses provided by the
	 * driver, starting where we left off.
	 */
	for ( i = isa->probe_idx - isa_extra_probe_addr_count ;
	      i < driver->addr_count ; i++ ) {

		/* If we've already used this device, skip it */
		if ( isa->already_tried ) {
			isa->already_tried = 0;
			continue;
		}

		/* Set I/O address */
		ioaddr = driver->probe_addrs[i];

		/* Use probe_addr method to see if there's a device
		 * present at this address.
		 */
		if ( driver->probe_addr ( ioaddr ) ) {
			isa->probe_idx = i + isa_extra_probe_addr_count;
			goto found;
		}
	}

 notfound:
	/* No device found */
	isa->probe_idx = 0;
	return 0;

 found:
	DBG ( "ISA found %s device at address %hx\n", driver->name, ioaddr );
	isa->ioaddr = ioaddr;
	isa->already_tried = 1;
	return 1;
}

/*
 * Find the next ISA device that can be used to boot using the
 * specified driver.
 *
 */
int find_isa_boot_device ( struct dev *dev, struct isa_driver *driver ) {
	struct isa_device *isa = ( struct isa_device * )dev->bus;

	if ( ! find_isa_device ( isa, driver ) )
		return 0;

	dev->name = driver->name;
	dev->devid.bus_type = ISA_BUS_TYPE;
	dev->devid.vendor_id = driver->mfg_id;
	dev->devid.device_id = driver->prod_id;

	return 1;
}
