/*
 * Split out from 3c509.c, since EISA cards are relatively rare, and
 * ROM space in 3c509s is very limited.
 *
 */

#include "eisa.h"
#include "isa.h"
#include "console.h"
#include "3c509.h"

/*
 * The EISA probe function
 *
 */
static int el3_eisa_probe ( struct dev *dev, struct eisa_device *eisa ) {
	struct nic *nic = nic_device ( dev );
	
	enable_eisa_device ( eisa );
	nic->ioaddr = eisa->ioaddr;
	nic->irqno = 0;
	printf ( "3C5x9 board on EISA at %#hx - ", nic->ioaddr );

	/* Hand off to generic t5x9 probe routine */
	return t5x9_probe ( nic, ISA_PROD_ID ( PROD_ID ), ISA_PROD_ID_MASK );
}

static struct eisa_id el3_eisa_adapters[] = {
	{ "3Com 3c509 EtherLink III (EISA)", MFG_ID, PROD_ID },
};

static struct eisa_driver el3_eisa_driver =
	EISA_DRIVER ( "3c509 (EISA)", el3_eisa_adapters );

BOOT_DRIVER ( "3c509 (EISA)", find_eisa_boot_device, el3_eisa_driver,
	      el3_eisa_probe );

ISA_ROM ( "3c509-eisa","3c509 (EISA)" );
