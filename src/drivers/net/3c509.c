/*
 * Split out into 3c509.c and 3c5x9.c, to make it possible to build a
 * 3c529 module without including ISA, ISAPnP and EISA code.
 *
 */

#include "eisa.h"
#include "isa.h"
#include "dev.h"
#include "io.h"
#include "timer.h"
#include "string.h"
#include "etherboot.h"
#include "3c509.h"

/*
 * 3c509 cards have their own method of contention resolution; this
 * effectively defines another bus type.
 *
 */

/*
 * A physical t509 device
 *
 */
struct t509_device {
	char *magic; /* must be first */
	struct dev *dev;
	uint16_t id_port;
	uint16_t ioaddr;
	unsigned char current_tag;
};

/*
 * A t509 driver
 *
 */
struct t509_driver {
	char *name;
};

/*
 * Ensure that there is sufficient space in the shared dev_bus
 * structure for a struct pci_device.
 *
 */
DEV_BUS( struct t509_device, t509_dev );
static char t509_magic[0]; /* guaranteed unique symbol */

/*
 * Find a port that can be used for contention select
 *
 * Called only once, so inlined for efficiency.
 *
 */
static inline int find_id_port ( struct t509_device *t509 ) {
	for ( t509->id_port = EP_ID_PORT_START ;
	      t509->id_port < EP_ID_PORT_END ;
	      t509->id_port += EP_ID_PORT_INC ) {
		outb ( 0x00, t509->id_port );
		outb ( 0xff, t509->id_port );
		if ( inb ( t509->id_port ) & 0x01 ) {
			/* Found a suitable port */
			return 1;
		}
	}
	/* No id port available */
	return 0;
}

/*
 * Send ID sequence to the ID port
 *
 * Called only once, so inlined for efficiency.
 *
 */
static inline void send_id_sequence ( struct t509_device *t509 ) {
	unsigned short lrs_state, i;

	outb ( 0x00, t509->id_port );
        outb ( 0x00, t509->id_port );
	lrs_state = 0xff;
        for ( i = 0; i < 255; i++ ) {
                outb ( lrs_state, t509->id_port );
                lrs_state <<= 1;
                lrs_state = lrs_state & 0x100 ? lrs_state ^ 0xcf : lrs_state;
        }
}

/*
 * We get eeprom data from the id_port given an offset into the eeprom.
 * Basically; after the ID_sequence is sent to all of the cards; they enter
 * the ID_CMD state where they will accept command requests. 0x80-0xbf loads
 * the eeprom data.  We then read the port 16 times and with every read; the
 * cards check for contention (ie: if one card writes a 0 bit and another
 * writes a 1 bit then the host sees a 0. At the end of the cycle; each card
 * compares the data on the bus; if there is a difference then that card goes
 * into ID_WAIT state again). In the meantime; one bit of data is returned in
 * the AX register which is conveniently returned to us by inb().  Hence; we
 * read 16 times getting one bit of data with each read.
 */
static uint16_t id_read_eeprom ( struct t509_device *t509, int offset ) {
	int i, data = 0;

	outb ( 0x80 + offset, t509->id_port );
	/* Do we really need this wait? Won't be noticeable anyway */
	udelay(10000);

	for ( i = 0; i < 16; i++ ) {
		data = ( data << 1 ) | ( inw ( t509->id_port ) & 1 );
	}
	return data;
}

/*
 * Find the next t509 device
 *
 * Called only once, so inlined for efficiency.
 *
 */
static inline int fill_t509_device ( struct t509_device *t509 ) {
	int i;
	uint16_t iobase;

	/* 
	 * If this is the start of the scan, find an id_port and clear
	 * all tag registers.  Otherwise, tell already-found NICs not
	 * to respond.
	 *
	 */
	if ( t509->current_tag == 0 ) {
		if ( ! find_id_port ( t509 ) ) {
			DBG ( "No ID port available for contention select\n" );
			return 0;
		}
		outb ( 0xd0, t509->id_port );
	} else {
		outb ( 0xd8, t509->id_port ) ;
	}

	/* Send the ID sequence */
	send_id_sequence ( t509 );

	/* Check the manufacturer ID */
	if ( id_read_eeprom ( t509, EEPROM_MFG_ID ) != MFG_ID ) {
		/* No more t509 devices */
		return 0;
	}

	/* Do contention select by reading the MAC address */
	for ( i = 0 ; i < 3 ; i++ ) {
		id_read_eeprom ( t509, i );
	}

	/* By now, only one device will be left active.  Get its I/O
	 * address, tag and activate the adaptor.  Tagging will
	 * prevent it taking part in the next scan, enabling us to see
	 * the next device.
	 */
	iobase = id_read_eeprom ( t509, EEPROM_ADDR_CFG );
	t509->ioaddr = 0x200 + ( ( iobase & 0x1f ) << 4 );
	outb ( ++t509->current_tag, t509->id_port ); /* tag */
	outb ( ( 0xe0 | iobase ), t509->id_port ); /* activate */

	return 1;
}

/*
 * Obtain a struct t509_device * from a struct dev *
 *
 * If dev has not previously been used for a PCI device scan, blank
 * out struct t509_device
 */
static struct t509_device * t509_device ( struct dev *dev ) {
	struct t509_device *t509 = dev->bus;

	if ( t509->magic != t509_magic ) {
		memset ( t509, 0, sizeof ( *t509 ) );
		t509->magic = t509_magic;
	}
	t509->dev = dev;
	return t509;
}

/*
 * Find a t509 device matching the specified driver.  ("Matching the
 * specified driver" is, in this case, a no-op, but we want to
 * preserve the common bus API).
 *
 */
static int find_t509_device ( struct t509_device *t509,
				 struct t509_driver *driver ) {
	/* Find the next t509 device */
	if ( ! fill_t509_device ( t509 ) )
		return 0;
	
	/* Fill in dev structure, if present */
	if ( t509->dev ) {
		t509->dev->name = driver->name;
		t509->dev->devid.bus_type = ISA_BUS_TYPE;
		t509->dev->devid.vendor_id = MFG_ID;
		t509->dev->devid.device_id = PROD_ID;
	}

	return 1;
}

/*
 * The ISA probe function
 *
 */
static struct t509_driver el3_t509_driver = { "3c509 (ISA)" };

static int el3_t509_probe ( struct dev *dev ) {
	struct nic *nic = nic_device ( dev );
	struct t509_device *t509 = t509_device ( dev );
	
	if ( ! find_t509_device ( t509, &el3_t509_driver ) )
		return 0;
	
	nic->ioaddr = t509->ioaddr;
	nic->irqno = 0;
	printf ( "3C5x9 board on ISA at %#hx - ", nic->ioaddr );

	/* Hand off to generic t5x9 probe routine */
	return t5x9_probe ( nic, ISA_PROD_ID ( PROD_ID ), ISA_PROD_ID_MASK );
}

BOOT_DRIVER ( "3c509", el3_t509_probe );

/*
 * The 3c509 driver also supports EISA cards
 *
 */
static struct eisa_id el3_eisa_adapters[] = {
	{ "3Com 3c509 EtherLink III (EISA)", MFG_ID, PROD_ID },
};

static struct eisa_driver el3_eisa_driver =
	EISA_DRIVER ( "3c509 (EISA)", el3_eisa_adapters );

static int el3_eisa_probe ( struct dev *dev ) {
	struct nic *nic = nic_device ( dev );
	struct eisa_device *eisa = eisa_device ( dev );
	
	if ( ! find_eisa_device ( eisa, &el3_eisa_driver ) )
		return 0;
	
	enable_eisa_device ( eisa );
	nic->ioaddr = eisa->ioaddr;
	nic->irqno = 0;
	printf ( "3C5x9 board on EISA at %#hx - ", nic->ioaddr );

	/* Hand off to generic t5x9 probe routine */
	return t5x9_probe ( nic, ISA_PROD_ID ( PROD_ID ), ISA_PROD_ID_MASK );
}

BOOT_DRIVER ( "3c509 (EISA)", el3_eisa_probe );

/*
 * We currently build both ISA and EISA support into a single ROM
 * image, though there's no reason why this couldn't be split to
 * reduce code size; just split this .c file into two in the obvious
 * place.
 *
 */
ISA_ROM ( "3c509","3c509, ISA/EISA" );

