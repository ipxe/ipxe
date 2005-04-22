/*
 * MCA bus driver code
 *
 * Abstracted from 3c509.c.
 *
 */

#include "string.h"
#include "io.h"
#include "console.h"
#include "mca.h"

/*
 * Increment a bus_loc structure to the next possible MCA location.
 * Leave the structure zeroed and return 0 if there are no more valid
 * locations.
 *
 */
static int mca_next_location ( struct bus_loc *bus_loc ) {
	struct mca_loc *mca_loc = ( struct mca_loc * ) bus_loc;
	
	/*
	 * Ensure that there is sufficient space in the shared bus
	 * structures for a struct mca_loc and a struct
	 * mca_dev, as mandated by bus.h.
	 *
	 */
	BUS_LOC_CHECK ( struct mca_loc );
	BUS_DEV_CHECK ( struct mca_device );

	return ( mca_loc->slot = ( ++mca_loc->slot & MCA_MAX_SLOT_NR ) );
}

/*
 * Fill in parameters for an MCA device based on slot number
 *
 */
static int mca_fill_device ( struct bus_dev *bus_dev,
			     struct bus_loc *bus_loc ) {
	struct mca_loc *mca_loc = ( struct mca_loc * ) bus_loc;
	struct mca_device *mca = ( struct mca_device * ) bus_dev;
	unsigned int i, seen_non_ff;

	/* Store slot in struct mca, set default values */
	mca->slot = mca_loc->slot;
	mca->name = "?";

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
 * Test whether or not a driver is capable of driving the device.
 *
 */
static int mca_check_driver ( struct bus_dev *bus_dev,
			      struct device_driver *device_driver ) {
	struct mca_device *mca = ( struct mca_device * ) bus_dev;
	struct mca_driver *driver
		= ( struct mca_driver * ) device_driver->bus_driver_info;
	unsigned int i;

	/* Compare against driver's ID list */
	for ( i = 0 ; i < driver->id_count ; i++ ) {
		struct mca_id *id = &driver->ids[i];
		
		if ( MCA_ID ( mca ) == id->id ) {
			DBG ( "MCA found ID %hx (device %s) "
			      "matching driver %s\n",
			      id->name, id->id, driver->name );
			mca->name = id->name;
			return 1;
		}
	}

	/* No device found */
	return 0;
}

/*
 * Describe an MCA device
 *
 */
static char * mca_describe ( struct bus_dev *bus_dev ) {
	struct mca_device *mca = ( struct mca_device * ) bus_dev;
	static char mca_description[] = "MCA 00";

	sprintf ( mca_description + 4, "%hhx", mca->slot );
	return mca_description;
}

/*
 * Name an MCA device
 *
 */
static const char * mca_name ( struct bus_dev *bus_dev ) {
	struct mca_device *mca = ( struct mca_device * ) bus_dev;
	
	return mca->name;
}

/*
 * MCA bus operations table
 *
 */
struct bus_driver mca_driver __bus_driver = {
	.next_location	= mca_next_location,
	.fill_device	= mca_fill_device,
	.check_driver	= mca_check_driver,
	.describe	= mca_describe,
	.name		= mca_name,
};

/*
 * Fill in a nic structure
 *
 */
void mca_fill_nic ( struct nic *nic, struct mca_device *mca ) {

	/* ioaddr and irqno must be read in a device-dependent way
	 * from the POS registers
	 */
	nic->ioaddr = 0;
	nic->irqno = 0;

	/* Fill in DHCP device ID structure */
	nic->dhcp_dev_id.bus_type = MCA_BUS_TYPE;
	nic->dhcp_dev_id.vendor_id = htons ( GENERIC_MCA_VENDOR );
	nic->dhcp_dev_id.device_id = htons ( MCA_ID ( mca ) );
}
