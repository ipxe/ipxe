/*
 * Split out from 3c509.c to make build process more sane
 *
 */

#include "etherboot.h"
#include "mca.h"
#include "isa.h"
#include "nic.h"
#include "3c509.h"

/*
 * This table and several other pieces of the MCA support
 * code were shamelessly borrowed from the Linux kernel source.
 *
 * MCA support added by Adam Fritzler (mid@auk.cx)
 *
 */
static struct mca_id el3_mca_adapters[] = {
        { "3Com 3c529 EtherLink III (10base2)", 0x627c },
        { "3Com 3c529 EtherLink III (10baseT)", 0x627d },
        { "3Com 3c529 EtherLink III (test mode)", 0x62db },
        { "3Com 3c529 EtherLink III (TP or coax)", 0x62f6 },
        { "3Com 3c529 EtherLink III (TP)", 0x62f7 },
};

static struct mca_driver t529_driver
	= MCA_DRIVER ( "3c529", el3_mca_adapters );

ISA_ROM( "3c529", "3c529 == MCA 3c509" );

static int t529_probe ( struct dev *dev ) {
	struct nic *nic = nic_device ( dev );
	struct mca_device *mca = mca_device ( dev );

	if ( ! find_mca_device ( mca, &t529_driver ) )
		return 0;

	/* Retrieve NIC parameters from MCA device parameters */
	nic->ioaddr = ( ( mca->pos[4] & 0xfc ) | 0x02 ) << 8;
	nic->irqno = mca->pos[5] & 0x0f;
	printf ( "%s board found on MCA at %#hx IRQ %d -",
		 dev->name, nic->ioaddr, nic->irqno );

	/* Hand off to generic t5x9 probe routine */
	return t5x9_probe ( nic, MCA_ID ( mca ), 0xffff );
}

BOOT_DRIVER ( "3c529", t529_probe );
