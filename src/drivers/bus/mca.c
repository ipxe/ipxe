/*
 * MCA bus driver code
 *
 * Abstracted from 3c509.c.
 *
 */

#include "etherboot.h"
#include "io.h"
#include "mca.h"

/*
 * Ensure that there is sufficient space in the shared dev_bus
 * structure for a struct pci_device.
 *
 */
DEV_BUS( struct mca_device, mca_dev );
static char mca_magic[0]; /* guaranteed unique symbol */

/*
 * Fill in parameters for an MCA device based on slot number
 *
 */
static int fill_mca_device ( struct mca_device *mca ) {
	unsigned int i, seen_non_ff;

	/* Make sure motherboard setup is off */
	outb_p ( 0xff, MCA_MOTHERBOARD_SETUP_REG );

	/* Select the slot */
	outb_p ( 0x8 | ( mca->slot & 0xf ), MCA_ADAPTER_SETUP_REG );

	/* Read the POS registers */
	seen_non_ff = 0;
	for ( i = 0 ; i < ( sizeof ( mca->pos ) / sizeof ( mca->pos[0] ) ) ;
	      i++ ) {
		mca->pos[i] = inb_p ( MCA_POS_REG ( i ) );
		if ( mca->pos[i] != 0xff )
			seen_non_ff = 1;
	}
	
	/* If all POS registers are 0xff, this means there's no device
	 * present
	 */
	if ( ! seen_non_ff )
		return 0;

	/* Kill all setup modes */
	outb_p ( 0, MCA_ADAPTER_SETUP_REG );

	DBG ( "MCA found slot %d id %hx "
	      "(POS %hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx)\n",
	      mca->slot, MCA_ID ( mca ),
	      mca->pos[0], mca->pos[1], mca->pos[2], mca->pos[3],
	      mca->pos[4], mca->pos[5], mca->pos[6], mca->pos[7] );

	return 1;
}

/*
 * Find an MCA device matching the specified driver
 *
 */
int find_mca_device ( struct mca_device *mca, struct mca_driver *driver ) {
	unsigned int i;

	/* Initialise struct mca if it's the first time it's been used. */
	if ( mca->magic != mca_magic ) {
		memset ( mca, 0, sizeof ( *mca ) );
		mca->magic = mca_magic;
	}

	/* Iterate through all possible MCA slots, starting where we
	 * left off
	 */
	DBG ( "MCA searching for device matching driver %s\n", driver->name );
	for ( ; mca->slot < MCA_MAX_SLOT_NR ; mca->slot++ ) {
		/* If we've already used this device, skip it */
		if ( mca->already_tried ) {
			mca->already_tried = 0;
			continue;
		}

		/* Fill in device parameters */
		if ( ! fill_mca_device ( mca ) ) {
			continue;
		}

		/* Compare against driver's ID list */
		for ( i = 0 ; i < driver->id_count ; i++ ) {
			struct mca_id *id = &driver->ids[i];

			if ( MCA_ID ( mca ) == id->id ) {
				DBG ( "MCA found ID %hx (device %s) "
				      "matching driver %s\n",
				      id->name, id->id, driver->name );
				mca->name = id->name;
				mca->already_tried = 1;
				return 1;
			}
		}
	}

	/* No device found */
	DBG ( "MCA found no device matching driver %s\n", driver->name );
	mca->slot = 0;
	return 0;
}

/*
 * Find the next MCA device that can be used to boot using the
 * specified driver.
 *
 */
int find_mca_boot_device ( struct dev *dev, struct mca_driver *driver ) {
	struct mca_device *mca = ( struct mca_device * )dev->bus;

	if ( ! find_mca_device ( mca, driver ) )
		return 0;

	dev->name = mca->name;
	dev->devid.bus_type = MCA_BUS_TYPE;
	dev->devid.vendor_id = GENERIC_MCA_VENDOR;
	dev->devid.device_id = MCA_ID ( mca );

	return 1;
}
