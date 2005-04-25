#include "string.h"
#include "console.h"
#include "config/isa.h"
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
 * (If ISA_PROBE_ADDRS ends with a zero, the driver's own list will
 * never be used).
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
#  define		ISA_PROBE_IDX_LIMIT	isa_extra_probe_addr_count
#else
#  define		ISA_PROBE_IDX_LIMIT	( ISA_MAX_PROBE_IDX + 1 )
#endif

/*
 * Increment a bus_loc structure to the next possible ISA location.
 * Leave the structure zeroed and return 0 if there are no more valid
 * locations.
 *
 */
static int isa_next_location ( struct bus_loc *bus_loc ) {
	struct isa_loc *isa_loc = ( struct isa_loc * ) bus_loc;
	
	/*
	 * Ensure that there is sufficient space in the shared bus
	 * structures for a struct isa_loc and a struct
	 * isa_dev, as mandated by bus.h.
	 *
	 */
	BUS_LOC_CHECK ( struct isa_loc );
	BUS_DEV_CHECK ( struct isa_device );

	return ( ( ++isa_loc->probe_idx < ISA_PROBE_IDX_LIMIT ) ?
		 1 : ( isa_loc->probe_idx = 0 ) );
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

	driver_probe_idx = isa_loc->probe_idx - isa_extra_probe_addr_count;
	if ( driver_probe_idx < 0 ) {
		isa->ioaddr = isa_extra_probe_addrs[isa_loc->probe_idx];
	} else {
		isa->ioaddr = 0;
		isa->driver_probe_idx = driver_probe_idx;
	}
	isa->mfg_id = isa->prod_id = 0;
	isa->name = "?";
	return 1;
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

	/* If ioaddr is zero, it means we're using a driver-specified
	 * ioaddr
	 */
	if ( ! isa->ioaddr ) {
		if ( isa->driver_probe_idx >= driver->addr_count )
			return 0;
		isa->ioaddr = driver->probe_addrs[isa->driver_probe_idx];
	}

	/* Use probe_addr method to see if there's a device
	 * present at this address.
	 */
	if ( driver->probe_addr ( isa->ioaddr ) ) {
		DBG ( "ISA found %s device at address %hx\n",
		      driver->name, isa->ioaddr );
		isa->name = driver->name;
		isa->mfg_id = driver->mfg_id;
		isa->prod_id = driver->prod_id;
		return 1;
	}

	/* No device found */
	return 0;
}

/*
 * Describe a ISA device
 *
 */
static char * isa_describe_device ( struct bus_dev *bus_dev ) {
	struct isa_device *isa = ( struct isa_device * ) bus_dev;
	static char isa_description[] = "ISA 0000";

	sprintf ( isa_description + 4, "%hx", isa->ioaddr );
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
