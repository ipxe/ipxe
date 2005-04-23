/*
 * Split out into 3c509.c and 3c5x9.c, to make it possible to build a
 * 3c529 module without including ISA, ISAPnP and EISA code.
 *
 */

#include "isa.h"
#include "io.h"
#include "timer.h"
#include "string.h"
#include "console.h"
#include "3c509.h"

/*
 * 3c509 cards have their own method of contention resolution; this
 * effectively defines another bus type similar to ISAPnP.  Even the
 * original ISA cards can be programatically mapped to any I/O address
 * in the range 0x200-0x3e0.
 * 
 * However, there is a small problem: once you've activated a card,
 * the only ways to deactivate it will also wipe its tag, meaning that
 * you won't be able to subsequently reactivate it without going
 * through the whole ID sequence again.  The solution we adopt is to
 * isolate and tag all cards at the start, and to immediately
 * re-isolate and re-tag a card after disabling it.
 *
 */

static uint16_t t509_id_port = 0;
static uint8_t t509_max_tag = 0;

/*
 * A location on a t509 bus
 *
 */
struct t509_loc {
	uint8_t tag;
};

/*
 * A physical t509 device
 *
 */
struct t509_device {
	uint16_t ioaddr;
	uint8_t tag;
};

/*
 * t509 utility functions
 *
 */

static inline void t509_set_id_port ( void ) {
	outb ( 0x00, t509_id_port );
}

static inline void t509_wait_for_id_sequence ( void ) {
	outb ( 0x00, t509_id_port );
}

static inline void t509_global_reset ( void ) {
	outb ( 0xc0, t509_id_port );
}

static inline void t509_reset_tag ( void ) {
	outb ( 0xd0, t509_id_port );
}

static inline void t509_set_tag ( uint8_t tag ) {
	outb ( 0xd0 | tag, t509_id_port );
}

static inline void t509_select_tag ( uint8_t tag ) {
	outb ( 0xd8 | tag, t509_id_port );
}

static inline void t509_activate ( uint16_t ioaddr ) {
	outb ( 0xe0 | ( ioaddr >> 4 ), t509_id_port );
}

static inline void t509_deactivate_and_reset_tag ( uint16_t ioaddr ) {
	outb ( GLOBAL_RESET, ioaddr + EP_COMMAND );
}

static inline void t509_load_eeprom_word ( uint8_t offset ) {
	outb ( 0x80 | offset, t509_id_port );
}

/*
 * Find a suitable ID port
 *
 */
static inline int t509_find_id_port ( void ) {

	for ( t509_id_port = EP_ID_PORT_START ;
	      t509_id_port < EP_ID_PORT_END ;
	      t509_id_port += EP_ID_PORT_INC ) {
		t509_set_id_port ();
		/* See if anything's listening */
		outb ( 0xff, t509_id_port );
		if ( inb ( t509_id_port ) & 0x01 ) {
			/* Found a suitable port */
			DBG ( "T509 using ID port at %hx\n", t509_id_port );
			return 1;
		}
	}
	/* No id port available */
	DBG ( "T509 found no available ID port\n" );
	return 0;
}

/*
 * Send ID sequence to the ID port
 *
 */
static void t509_send_id_sequence ( void ) {
	unsigned short lrs_state, i;

	t509_set_id_port ();
	/* Reset IDS on cards */
	t509_wait_for_id_sequence ();
	lrs_state = 0xff;
        for ( i = 0; i < 255; i++ ) {
                outb ( lrs_state, t509_id_port );
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
static uint16_t t509_id_read_eeprom ( int offset ) {
	int i, data = 0;

	t509_load_eeprom_word ( offset );
	/* Do we really need this wait? Won't be noticeable anyway */
	udelay(10000);

	for ( i = 0; i < 16; i++ ) {
		data = ( data << 1 ) | ( inw ( t509_id_port ) & 1 );
	}
	return data;
}

/*
 * Isolate and tag all t509 cards
 *
 */
static void t509_isolate ( void ) {
	unsigned int i;
	uint16_t contend[3];

	/* Find a suitable ID port */
	if ( ! t509_find_id_port () )
		return;

	while ( 1 ) {

		/* All cards are in ID_WAIT state each time we go
		 * through this loop.
		 */

		/* Send the ID sequence */
		t509_send_id_sequence ();

		/* First time through, reset all tags.  On subsequent
		 * iterations, kill off any already-tagged cards
		 */
		if ( t509_max_tag == 0 ) {
			t509_reset_tag();
		} else {
			t509_select_tag(0);
		}
	
		/* Read the manufacturer ID, to see if there are any
		 * more cards
		 */
		if ( t509_id_read_eeprom ( EEPROM_MFG_ID ) != MFG_ID ) {
			DBG ( "T509 saw %s signs of life\n",
			      t509_max_tag ? "no further" : "no" );
			break;
		}

		/* Perform contention selection on the MAC address */
		for ( i = 0 ; i < 3 ; i++ ) {
			contend[i] = t509_id_read_eeprom ( i );
		}

		/* Only one device will still be left alive.  Tag it. */
		++t509_max_tag;
		DBG ( "T509 found card %hx%hx%hx, assigning tag %hhx\n",
		      contend[0], contend[1], contend[2], t509_max_tag );
		t509_set_tag ( t509_max_tag );

		/* Return all cards back to ID_WAIT state */
		t509_wait_for_id_sequence();
	}

	DBG ( "T509 found %d cards using ID port %hx\n",
	      t509_max_tag, t509_id_port );
	return;
}

/*
 * Increment a bus_loc structure to the next possible T509 location.
 * Leave the structure zeroed and return 0 if there are no more valid
 * locations.
 *
 */
static int t509_next_location ( struct bus_loc *bus_loc ) {
	struct t509_loc *t509_loc = ( struct t509_loc * ) bus_loc;
	
	/*
	 * Ensure that there is sufficient space in the shared bus
	 * structures for a struct t509_loc and a struct t509_dev,
	 * as mandated by bus.h.
	 *
	 */
	BUS_LOC_CHECK ( struct t509_loc );
	BUS_DEV_CHECK ( struct t509_device );

	return ( t509_loc->tag = ( ++t509_loc->tag & EP_TAG_MAX ) );
}

/*
 * Fill in parameters for a T509 device based on tag
 *
 * Return 1 if device present, 0 otherwise
 *
 */
static int t509_fill_device ( struct bus_dev *bus_dev,
			      struct bus_loc *bus_loc ) {
	struct t509_device *t509 = ( struct t509_device * ) bus_dev;
	struct t509_loc *t509_loc = ( struct t509_loc * ) bus_loc;
	uint16_t iobase;

	/* Copy tag to struct t509 */
	t509->tag = t509_loc->tag;

	/* Tag 0 is never valid, but may be passed in */
	if ( ! t509->tag )
		return 0;

	/* Perform isolation if it hasn't yet been done */
	if ( ! t509_id_port )
		t509_isolate();

	/* Check tag is in range */
	if ( t509->tag > t509_max_tag )
		return 0;

	/* Send the ID sequence */
	t509_send_id_sequence ();

	/* Select the specified tag */
	t509_select_tag ( t509->tag );

	/* Read the default I/O address */
	iobase = t509_id_read_eeprom ( EEPROM_ADDR_CFG );
	t509->ioaddr = 0x200 + ( ( iobase & 0x1f ) << 4 );

	/* Send card back to ID_WAIT */
	t509_wait_for_id_sequence();

	DBG ( "T509 found device %hhx, base %hx\n", t509->tag, t509->ioaddr );
	return 1;
}

/*
 * Test whether or not a driver is capable of driving the device.
 * This is a no-op for T509.
 *
 */
static int t509_check_driver ( struct bus_dev *bus_dev __unused,
			       struct device_driver *device_driver __unused ) {
	return 1;
}

/*
 * Describe a T509 device
 *
 */
static char * t509_describe ( struct bus_dev *bus_dev ) {
	struct t509_device *t509 = ( struct t509_device * ) bus_dev;
	static char t509_description[] = "T509 00";

	sprintf ( t509_description + 4, "%hhx", t509->tag );
	return t509_description;
}

/*
 * Name a T509 device
 *
 */
static const char * t509_name ( struct bus_dev *bus_dev __unused ) {
	return "T509";
}

/*
 * T509 bus operations table
 *
 */
static struct bus_driver t509_driver __bus_driver = {
	.next_location	= t509_next_location,
	.fill_device	= t509_fill_device,
	.check_driver	= t509_check_driver,
	.describe	= t509_describe,
	.name		= t509_name,
};

/*
 * Activate a T509 device
 *
 * The device will be enabled at whatever ioaddr is specified in the
 * struct t509_device; there is no need to stick with the default
 * ioaddr read from the EEPROM.
 *
 */
static inline void activate_t509_device ( struct t509_device *t509 ) {
	t509_send_id_sequence ();
	t509_select_tag ( t509->tag );
	t509_activate ( t509->ioaddr );
	DBG ( "T509 activated device %hhx at ioaddr %hx\n",
	      t509->tag, t509->ioaddr );
}

/*
 * Deactivate a T509 device
 *
 * Disabling also clears the tag, so we immediately isolate and re-tag
 * this card.
 *
 */
static inline void deactivate_t509_device ( struct t509_device *t509 ) {
	t509_deactivate_and_reset_tag ( t509->ioaddr );
	udelay ( 1000 );
	t509_send_id_sequence ();
	t509_select_tag ( 0 );
	t509_set_tag ( t509->tag );
	t509_wait_for_id_sequence ();
	DBG ( "T509 deactivated device at %hx and re-tagged as %hhx\n",
	      t509->ioaddr, t509->tag );
}

/*
 * Fill in a nic structure
 *
 * Called only once, so inlined for efficiency
 *
 */
static inline void t509_fill_nic ( struct nic *nic,
				   struct t509_device *t509 ) {

	/* Fill in ioaddr and irqno */
	nic->ioaddr = t509->ioaddr;
	nic->irqno = 0;

	/* Fill in DHCP device ID structure */
	nic->dhcp_dev_id.bus_type = ISA_BUS_TYPE;
	nic->dhcp_dev_id.vendor_id = htons ( MFG_ID );
	nic->dhcp_dev_id.device_id = htons ( PROD_ID );
}

/*
 * The ISA probe function
 *
 */
static int el3_t509_probe ( struct nic *nic, struct t509_device *t509 ) {
	
	/* We could change t509->ioaddr if we wanted to */
	activate_t509_device ( t509 );
	t509_fill_nic ( nic, t509 );

	/* Hand off to generic t5x9 probe routine */
	return t5x9_probe ( nic, ISA_PROD_ID ( PROD_ID ), ISA_PROD_ID_MASK );
}

static void el3_t509_disable ( struct nic *nic, struct t509_device *t509 ) {
	t5x9_disable ( nic );
	deactivate_t509_device ( t509 );
}

static struct {} el3_t509_driver;

DRIVER ( "3c509", nic_driver, t509_driver, el3_t509_driver,
	 el3_t509_probe, el3_t509_disable );

ISA_ROM ( "3c509", "3c509" );
