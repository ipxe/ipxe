#include "etherboot.h"
#include "dev.h"
#include "io.h"
#include "timer.h"
#include "eisa.h"

/*
 * Ensure that there is sufficient space in the shared dev_bus
 * structure for a struct pci_device.
 *
 */
DEV_BUS( struct eisa_device, eisa_dev );
static char eisa_magic[0]; /* guaranteed unique symbol */

/*
 * Fill in parameters for an EISA device based on slot number
 *
 * Return 1 if device present, 0 otherwise
 *
 */
static int fill_eisa_device ( struct eisa_device *eisa ) {
	uint8_t present;

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

	DBG ( "EISA slot %d (base %#hx) ID %hx:%hx (\"%s\")\n",
	      eisa->slot, eisa->ioaddr, eisa->mfg_id, eisa->prod_id,
	      isa_id_string ( eisa->mfg_id, eisa->prod_id ) );

	return 1;
}

/*
 * Obtain a struct eisa * from a struct dev *
 *
 * If dev has not previously been used for an EISA device scan, blank
 * out struct eisa
 */
struct eisa_device * eisa_device ( struct dev *dev ) {
	struct eisa_device *eisa = dev->bus;;

	if ( eisa->magic != eisa_magic ) {
		memset ( eisa, 0, sizeof ( *eisa ) );
		eisa->magic = eisa_magic;
	}
	eisa->dev = dev;
	return eisa;
}

/*
 * Find an EISA device matching the specified driver
 *
 */
int find_eisa_device ( struct eisa_device *eisa, struct eisa_driver *driver ) {
	unsigned int i;

	/* Iterate through all possible EISA slots, starting where we
	 * left off.  If eisa->slot is zero (which it will be if we
	 * have a zeroed structure), start from slot EISA_MIN_SLOT,
	 * since slot 0 doesn't exist.
	 */
	if ( ! eisa->slot ) {
		eisa->slot = EISA_MIN_SLOT;
	}
	for ( ; eisa->slot <= EISA_MAX_SLOT ; eisa->slot++ ) {
		/* If we've already used this device, skip it */
		if ( eisa->already_tried ) {
			eisa->already_tried = 0;
			continue;
		}

		/* Fill in device parameters */
		if ( ! fill_eisa_device ( eisa ) ) {
			continue;
		}

		/* Compare against driver's ID list */
		for ( i = 0 ; i < driver->id_count ; i++ ) {
			struct eisa_id *id = &driver->ids[i];
			
			if ( ( eisa->mfg_id == id->mfg_id ) &&
			     ( ISA_PROD_ID ( eisa->prod_id ) ==
			       ISA_PROD_ID ( id->prod_id ) ) ) {
				DBG ( "Device %s (driver %s) matches ID %s\n",
				      id->name, driver->name,
				      isa_id_string ( eisa->mfg_id,
						      eisa->prod_id ) );
				if ( eisa->dev ) {
					eisa->dev->name = driver->name;
					eisa->dev->devid.bus_type
						= ISA_BUS_TYPE;
					eisa->dev->devid.vendor_id
						= eisa->mfg_id;
					eisa->dev->devid.device_id
						= eisa->prod_id;
				}
				eisa->already_tried = 1;
				return 1;
			}
		}
	}

	/* No device found */
	eisa->slot = EISA_MIN_SLOT;
	return 0;
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
