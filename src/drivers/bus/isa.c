#include "string.h"
#include "console.h"
#include "config/isa.h"
#include "dev.h"
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
 * The ISA probe address list can be overridden by config.h; if the
 * user specifies ISA_PROBE_ADDRS then that list will be used first.
 * (If ISA_PROBE_ONLY is defined, the driver's own list will never be
 * used).
 */

/*
 * User-supplied probe address list
 *
 */
static isa_probe_addr_t isa_extra_probe_addrs[] = {
#ifdef ISA_PROBE_ADDRS
	ISA_PROBE_ADDRS
#endif
};
#define isa_extra_probe_addr_count \
     ( sizeof ( isa_extra_probe_addrs ) / sizeof ( isa_extra_probe_addrs[0] ) )

#ifdef ISA_PROBE_ONLY
#define ISA_PROBE_ADDR_COUNT(driver) ( isa_extra_probe_addr_count )
#else
#define ISA_PROBE_ADDR_COUNT(driver) \
	( isa_extra_probe_addr_count + (driver)->addr_count )
#endif

/*
 * Symbols defined by linker
 *
 */
static struct isa_driver isa_drivers[0]
	__table_start ( struct isa_driver, isa_driver );
static struct isa_driver isa_drivers_end[0]
	__table_end ( struct isa_driver, isa_driver );

/*
 * Increment a bus_loc structure to the next possible ISA location.
 * Leave the structure zeroed and return 0 if there are no more valid
 * locations.
 *
 * There is no sensible concept of a device location on an ISA bus, so
 * we use the probe address list for each ISA driver to define the
 * list of ISA locations.
 *
 */
static int isa_next_location ( struct bus_loc *bus_loc ) {
	struct isa_loc *isa_loc = ( struct isa_loc * ) bus_loc;
	struct isa_driver *driver;

	/*
	 * Ensure that there is sufficient space in the shared bus
	 * structures for a struct isa_loc and a struct
	 * isa_dev, as mandated by bus.h.
	 *
	 */
	BUS_LOC_CHECK ( struct isa_loc );
	BUS_DEV_CHECK ( struct isa_device );

	/* Move to next probe address within this driver */
	driver = &isa_drivers[isa_loc->driver];
	if ( ++isa_loc->probe_idx < ISA_PROBE_ADDR_COUNT ( driver ) )
		return 1;

	/* Move to next driver */
	isa_loc->probe_idx = 0;
	if ( ( ++isa_loc->driver, ++driver ) < isa_drivers_end )
		return 1;

	isa_loc->driver = 0;
	return 0;
}

/*
 * Fill in parameters (vendor & device ids, class, membase etc.) for
 * an ISA device based on bus_loc.
 *
 * Returns 1 if a device was found, 0 for no device present.
 *
 */
static int isa_fill_device ( struct bus_dev *bus_dev,
			     struct bus_loc *bus_loc ) {
	struct isa_loc *isa_loc = ( struct isa_loc * ) bus_loc;
	struct isa_device *isa = ( struct isa_device * ) bus_dev;
	signed int driver_probe_idx;
	
	/* Fill in struct isa from struct isa_loc */
	isa->driver = &isa_drivers[isa_loc->driver];
	driver_probe_idx = isa_loc->probe_idx - isa_extra_probe_addr_count;
	if ( driver_probe_idx < 0 ) {
		isa->ioaddr = isa_extra_probe_addrs[isa_loc->probe_idx];
	} else {
		isa->ioaddr = isa->driver->probe_addrs[driver_probe_idx];
	}

	/* Call driver's probe_addr method to determine if a device is
	 * physically present
	 */
	if ( isa->driver->probe_addr ( isa->ioaddr ) ) {
		isa->name = isa->driver->name;
		isa->mfg_id = isa->driver->mfg_id;
		isa->prod_id = isa->driver->prod_id;
		DBG ( "ISA found %s device at address %hx\n",
		      isa->name, isa->ioaddr );
		return 1;
	}

	return 0;
}

/*
 * Test whether or not a driver is capable of driving the specified
 * device.
 *
 */
int isa_check_driver ( struct bus_dev *bus_dev,
		       struct device_driver *device_driver ) {
	struct isa_device *isa = ( struct isa_device * ) bus_dev;
	struct isa_driver *driver
		= ( struct isa_driver * ) device_driver->bus_driver_info;

	return ( driver == isa->driver );
}

/*
 * Describe a ISA device
 *
 */
static char * isa_describe_device ( struct bus_dev *bus_dev ) {
	struct isa_device *isa = ( struct isa_device * ) bus_dev;
	static char isa_description[] = "ISA 0000 (00)";

	sprintf ( isa_description + 4, "%hx (%hhx)", isa->ioaddr,
		  isa->driver - isa_drivers );
	return isa_description;
}

/*
 * Name a ISA device
 *
 */
static const char * isa_name_device ( struct bus_dev *bus_dev ) {
	struct isa_device *isa = ( struct isa_device * ) bus_dev;
	
	return isa->name;
}

/*
 * ISA bus operations table
 *
 */
struct bus_driver isa_driver __bus_driver = {
	.name			= "ISA",
	.next_location		= isa_next_location,
	.fill_device		= isa_fill_device,
	.check_driver		= isa_check_driver,
	.describe_device	= isa_describe_device,
	.name_device		= isa_name_device,
};

/*
 * Fill in a nic structure
 *
 */
void isa_fill_nic ( struct nic *nic, struct isa_device *isa ) {

	/* Fill in ioaddr and irqno */
	nic->ioaddr = isa->ioaddr;
	nic->irqno = 0;

	/* Fill in DHCP device ID structure */
	nic->dhcp_dev_id.bus_type = ISA_BUS_TYPE;
	nic->dhcp_dev_id.vendor_id = htons ( isa->mfg_id );
	nic->dhcp_dev_id.device_id = htons ( isa->prod_id );
}
