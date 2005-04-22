#include "string.h"
#include "io.h"
#include "timer.h"
#include "console.h"
#include "eisa.h"

/*
 * Increment a bus_loc structure to the next possible EISA location.
 * Leave the structure zeroed and return 0 if there are no more valid
 * locations.
 *
 */
static int eisa_next_location ( struct bus_loc *bus_loc ) {
	struct eisa_loc *eisa_loc = ( struct eisa_loc * ) bus_loc;
	
	/*
	 * Ensure that there is sufficient space in the shared bus
	 * structures for a struct isa_loc and a struct
	 * isa_dev, as mandated by bus.h.
	 *
	 */
	BUS_LOC_CHECK ( struct eisa_loc );
	BUS_DEV_CHECK ( struct eisa_device );

	return ( ++eisa_loc->slot & EISA_MAX_SLOT );
}

/*
 * Fill in parameters for an EISA device based on slot number
 *
 * Return 1 if device present, 0 otherwise
 *
 */
static int eisa_fill_device  ( struct bus_dev *bus_dev,
			       struct bus_loc *bus_loc ) {
	struct eisa_loc *eisa_loc = ( struct eisa_loc * ) bus_loc;
	struct eisa_device *eisa = ( struct eisa_device * ) bus_dev;
	uint8_t present;

	/* Copy slot number to struct eisa, set default values */
	eisa->slot = eisa_loc->slot;
	eisa->name = "?";

	/* Slot 0 is never valid */
	if ( ! eisa->slot )
		return 0;

	/* Set ioaddr */
	eisa->ioaddr = EISA_SLOT_BASE ( eisa->slot );

	/* Test for board present */
	outb ( 0xff, eisa->ioaddr + EISA_MFG_ID_HI );
	present = inb ( eisa->ioaddr + EISA_MFG_ID_HI );
	if ( present & 0x80 ) {
		/* No board present */
		return 0;
	}

	/* Read mfg and product IDs.  Yes, the resulting uint16_ts
	 * will be upside-down.  This appears to be by design.
	 */
	eisa->mfg_id = ( inb ( eisa->ioaddr + EISA_MFG_ID_LO ) << 8 )
		+ present;
	eisa->prod_id = ( inb ( eisa->ioaddr + EISA_PROD_ID_LO ) << 8 )
		+ inb ( eisa->ioaddr + EISA_PROD_ID_HI );

	DBG ( "EISA found slot %hhx (base %#hx) ID %hx:%hx (\"%s\")\n",
	      eisa->slot, eisa->ioaddr, eisa->mfg_id, eisa->prod_id,
	      isa_id_string ( eisa->mfg_id, eisa->prod_id ) );

	return 1;
}

/*
 * Test whether or not a driver is capable of driving the device.
 *
 */
static int eisa_check_driver ( struct bus_dev *bus_dev,
				 struct device_driver *device_driver ) {
	struct eisa_device *eisa = ( struct eisa_device * ) bus_dev;
	struct eisa_driver *driver
		= ( struct eisa_driver * ) device_driver->bus_driver_info;
	unsigned int i;

	/* Compare against driver's ID list */
	for ( i = 0 ; i < driver->id_count ; i++ ) {
		struct eisa_id *id = &driver->ids[i];
		
		if ( ( eisa->mfg_id == id->mfg_id ) &&
		     ( ISA_PROD_ID ( eisa->prod_id ) ==
		       ISA_PROD_ID ( id->prod_id ) ) ) {
			DBG ( "EISA found ID %hx:%hx (\"%s\") "
			      "(device %s) matching driver %s\n",
			      eisa->mfg_id, eisa->prod_id,
			      isa_id_string ( eisa->mfg_id,
					      eisa->prod_id ),
			      id->name, driver->name );
			eisa->name = id->name;
			return 1;
		}
	}

	/* No device found */
	return 0;
}

/*
 * Describe an EISA device
 *
 */
static char * eisa_describe ( struct bus_dev *bus_dev ) {
	struct eisa_device *eisa = ( struct eisa_device * ) bus_dev;
	static char eisa_description[] = "EISA 00";

	sprintf ( eisa_description + 5, "%hhx", eisa->slot );
	return eisa_description;
}

/*
 * Name an EISA device
 *
 */
static const char * eisa_name ( struct bus_dev *bus_dev ) {
	struct eisa_device *eisa = ( struct eisa_device * ) bus_dev;
	
	return eisa->name;
}

/*
 * EISA bus operations table
 *
 */
struct bus_driver eisa_driver __bus_driver = {
	.next_location	= eisa_next_location,
	.fill_device	= eisa_fill_device,
	.check_driver	= eisa_check_driver,
	.describe	= eisa_describe,
	.name		= eisa_name,
};

/*
 * Fill in a nic structure
 *
 */
void eisa_fill_nic ( struct nic *nic, struct eisa_device *eisa ) {

	/* Fill in ioaddr and irqno */
	nic->ioaddr = eisa->ioaddr;
	nic->irqno = 0;

	/* Fill in DHCP device ID structure */
	nic->dhcp_dev_id.bus_type = ISA_BUS_TYPE;
	nic->dhcp_dev_id.vendor_id = htons ( eisa->mfg_id );
	nic->dhcp_dev_id.device_id = htons ( eisa->prod_id );
}

/*
 * Reset and enable an EISA device
 *
 */
void enable_eisa_device ( struct eisa_device *eisa ) {
	/* Set reset line high for 1000 µs.  Spec says 500 µs, but
	 * this doesn't work for all cards, so we are conservative.
	 */
	outb ( EISA_CMD_RESET, eisa->ioaddr + EISA_GLOBAL_CONFIG );
	udelay ( 1000 ); /* Must wait 800 */

	/* Set reset low and write a 1 to ENABLE.  Delay again, in
	 * case the card takes a while to wake up.
	 */
	outb ( EISA_CMD_ENABLE, eisa->ioaddr + EISA_GLOBAL_CONFIG );
	udelay ( 1000 ); /* Must wait 800 */
}
