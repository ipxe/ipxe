/**************************************************************************
*
*    isapnp.c -- Etherboot isapnp support for the 3Com 3c515
*    Written 2002-2003 by Timothy Legge <tlegge@rogers.com>
*
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software
*    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*    Portions of this code:
*	Copyright (C) 2001  P.J.H.Fox (fox@roestock.demon.co.uk)
*
*
*    REVISION HISTORY:
*    ================
*    Version 0.1 April 26, 2002 TJL
*    Version 0.2 01/08/2003	TJL Moved outside the 3c515.c driver file
*    Version 0.3 Sept 23, 2003	timlegge Change delay to currticks
*		
*
*    Generalised into an ISAPnP bus that can be used by more than just
*    the 3c515 by Michael Brown <mbrown@fensystems.co.uk>
*
***************************************************************************/

/** @file
 *
 * ISAPnP bus support
 *
 * Etherboot orignally gained ISAPnP support in a very limited way for
 * the 3c515 NIC.  The current implementation is almost a complete
 * rewrite based on the ISAPnP specification, with passing reference
 * to the Linux ISAPnP code.
 *
 * There can be only one ISAPnP bus in a system.  Once the read port
 * is known and all cards have been allocated CSNs, there's nothing to
 * be gained by re-scanning for cards.
 *
 * However, we shouldn't make scanning the ISAPnP bus an INIT_FN(),
 * because even ISAPnP probing can still screw up other devices on the
 * ISA bus.  We therefore probe only when we are first asked to find
 * an ISAPnP device.
 *
 * External code (e.g. the ISAPnP ROM prefix) may already know the
 * read port address, in which case it can store it in
 * #isapnp_read_port.  Note that setting the read port address in this
 * way will prevent further isolation from taking place; you should
 * set the read port address only if you know that devices have
 * already been allocated CSNs.
 *
 */

#include "string.h"
#include "timer.h"
#include "io.h"
#include "console.h"
#include "isapnp.h"

/**
 * ISAPnP Read Port address.
 *
 */
uint16_t isapnp_read_port;

/**
 * Highest assigned CSN.
 *
 * Note that @b we do not necessarily assign CSNs; it could be done by
 * the PnP BIOS instead.  We therefore set this only when we first try
 * to Wake[CSN] a device and find that there's nothing there.  Page 16
 * (PDF page 22) of the ISAPnP spec states that "Valid Card Select
 * Numbers for identified ISA cards range from 1 to 255 and must be
 * assigned sequentially starting from 1", so we are (theoretically,
 * at least) safe to assume that there are no ISAPnP cards at CSNs
 * higher than the first unused CSN.
 *
 */
static uint8_t isapnp_max_csn = 0xff;

/*
 * ISAPnP utility functions
 *
 */

#define ISAPNP_CARD_ID_FMT "ID %hx:%hx (\"%s\") serial %x"
#define ISAPNP_CARD_ID_DATA(identifier)					  \
	(identifier)->vendor_id, (identifier)->prod_id,			  \
	isa_id_string ( (identifier)->vendor_id, (identifier)->prod_id ), \
	(identifier)->serial
#define ISAPNP_DEV_ID_FMT "ID %hx:%hx (\"%s\")"
#define ISAPNP_DEV_ID_DATA(isapnp)					  \
	(isapnp)->vendor_id, (isapnp)->prod_id,				  \
	isa_id_string ( (isapnp)->vendor_id, (isapnp)->prod_id )

static inline void isapnp_write_address ( uint8_t address ) {
	outb ( address, ISAPNP_ADDRESS );
}

static inline void isapnp_write_data ( uint8_t data ) {
	outb ( data, ISAPNP_WRITE_DATA );
}

static inline uint8_t isapnp_read_data ( void ) {
	return inb ( isapnp_read_port );
}

static inline void isapnp_write_byte ( uint8_t address, uint8_t value ) {
	isapnp_write_address ( address );
	isapnp_write_data ( value );
}

static inline uint8_t isapnp_read_byte ( uint8_t address ) {
	isapnp_write_address ( address );
	return isapnp_read_data ();
}

static inline uint16_t isapnp_read_word ( uint8_t address ) {
	/* Yes, they're in big-endian order */
	return ( ( isapnp_read_byte ( address ) << 8 )
		 + isapnp_read_byte ( address + 1 ) );
}

/** Inform cards of a new read port address */
static inline void isapnp_set_read_port ( void ) {
	isapnp_write_byte ( ISAPNP_READPORT, isapnp_read_port >> 2 );
}

/**
 * Enter the Isolation state.
 *
 * Only cards currently in the Sleep state will respond to this
 * command.
 *
 */
static inline void isapnp_serialisolation ( void ) {
	isapnp_write_address ( ISAPNP_SERIALISOLATION );
}

/**
 * Enter the Wait for Key state.
 *
 * All cards will respond to this command, regardless of their current
 * state.
 *
 */
static inline void isapnp_wait_for_key ( void ) {
	isapnp_write_byte ( ISAPNP_CONFIGCONTROL, ISAPNP_CONFIG_WAIT_FOR_KEY );
}

/**
 * Reset (i.e. remove) Card Select Number.
 *
 * Only cards currently in the Sleep state will respond to this
 * command.
 *
 */
static inline void isapnp_reset_csn ( void ) {
	isapnp_write_byte ( ISAPNP_CONFIGCONTROL, ISAPNP_CONFIG_RESET_CSN );
}

/**
 * Place a specified card into the Config state.
 *
 * @v csn		Card Select Number
 * @ret None
 * @err None
 *
 * Only cards currently in the Sleep, Isolation, or Config states will
 * respond to this command.  The card that has the specified CSN will
 * enter the Config state, all other cards will enter the Sleep state.
 *
 */
static inline void isapnp_wake ( uint8_t csn ) {
	isapnp_write_byte ( ISAPNP_WAKE, csn );
}

static inline uint8_t isapnp_read_resourcedata ( void ) {
	return isapnp_read_byte ( ISAPNP_RESOURCEDATA );
}

static inline uint8_t isapnp_read_status ( void ) {
	return isapnp_read_byte ( ISAPNP_STATUS );
}

/**
 * Assign a Card Select Number to a card, and enter the Config state.
 *
 * @v csn		Card Select Number
 * @ret None
 * @err None
 *
 * Only cards in the Isolation state will respond to this command.
 * The isolation protocol is designed so that only one card will
 * remain in the Isolation state by the time the isolation protocol
 * completes.
 *
 */
static inline void isapnp_write_csn ( uint8_t csn ) {
	isapnp_write_byte ( ISAPNP_CARDSELECTNUMBER, csn );
}

static inline void isapnp_logicaldevice ( uint8_t logdev ) {
	isapnp_write_byte ( ISAPNP_LOGICALDEVICENUMBER, logdev );
}

static inline void isapnp_activate ( uint8_t logdev ) {
	isapnp_logicaldevice ( logdev );
	isapnp_write_byte ( ISAPNP_ACTIVATE, 1 );
}

static inline void isapnp_deactivate ( uint8_t logdev ) {
	isapnp_logicaldevice ( logdev );
	isapnp_write_byte ( ISAPNP_ACTIVATE, 0 );
}

static inline uint16_t isapnp_read_iobase ( unsigned int index ) {
	return isapnp_read_word ( ISAPNP_IOBASE ( index ) );
}

static inline uint8_t isapnp_read_irqno ( unsigned int index ) {
	return isapnp_read_byte ( ISAPNP_IRQNO ( index ) );
}

static void isapnp_delay ( void ) {
	udelay ( 1000 );
}

/**
 * Linear feedback shift register.
 *
 * @v lfsr		Current value of the LFSR
 * @v input_bit		Current input bit to the LFSR
 * @ret lfsr		Next value of the LFSR
 * @err None
 *
 * This routine implements the linear feedback shift register as
 * described in Appendix B of the PnP ISA spec.  The hardware
 * implementation uses eight D-type latches and two XOR gates.  I
 * think this is probably the smallest possible implementation in
 * software.  Six instructions when input_bit is a constant 0 (for
 * isapnp_send_key).  :)
 *
 */
static inline uint8_t isapnp_lfsr_next ( uint8_t lfsr, int input_bit ) {
	register uint8_t lfsr_next;

	lfsr_next = lfsr >> 1;
	lfsr_next |= ( ( ( lfsr ^ lfsr_next ) ^ input_bit ) ) << 7;
	return lfsr_next;
}

/**
 * Send the ISAPnP initiation key.
 *
 * Sending the key causes all ISAPnP cards that are currently in the
 * Wait for Key state to transition into the Sleep state.
 *
 */
static void isapnp_send_key ( void ) {
	unsigned int i;
	uint8_t lfsr;

	isapnp_delay();
	isapnp_write_address ( 0x00 );
	isapnp_write_address ( 0x00 );

	lfsr = ISAPNP_LFSR_SEED;
	for ( i = 0 ; i < 32 ; i++ ) {
		isapnp_write_address ( lfsr );
		lfsr = isapnp_lfsr_next ( lfsr, 0 );
	}
}

/**
 * Compute ISAPnP identifier checksum
 *
 * @v identifier		ISAPnP identifier
 * @ret checksum		Expected checksum value
 * @err None
 *
 */
static uint8_t isapnp_checksum ( struct isapnp_identifier *identifier ) {
	int i, j;
	uint8_t lfsr;
	uint8_t byte;

	lfsr = ISAPNP_LFSR_SEED;
	for ( i = 0 ; i < 8 ; i++ ) {
		byte = ( (char *) identifier )[i];
		for ( j = 0 ; j < 8 ; j++ ) {
			lfsr = isapnp_lfsr_next ( lfsr, byte );
			byte >>= 1;
		}
	}
	return lfsr;
}

/*
 * Read a byte of resource data from the current location
 *
 */
static inline uint8_t isapnp_peek_byte ( void ) {
	int i;

	/* Wait for data to be ready */
	for ( i = 0 ; i < 20 ; i ++ ) {
		if ( isapnp_read_status() & 0x01 ) {
			/* Byte ready - read it */
			return isapnp_read_resourcedata();
		}
		isapnp_delay ();
	}
	/* Data never became ready - return 0xff */
	return 0xff;
}

/**
 * Read resource data.
 *
 * @v buf		Buffer in which to store data, or NULL
 * @v bytes		Number of bytes to read
 * @ret None
 * @err None
 *
 * Resource data is read from the current location.  If #buf is NULL,
 * the data is discarded.
 *
 */
static void isapnp_peek ( uint8_t *buf, size_t bytes ) {
	unsigned int i;
	uint8_t byte;

	for ( i = 0 ; i < bytes ; i++) {
		byte = isapnp_peek_byte();
		if ( buf ) {
			buf[i] = byte;
		}
	}
}

/**
 * Find a tag within the resource data.
 *
 * @v wanted_tag	The tag that we're looking for
 * @v buf		Buffer in which to store the tag's contents
 * @ret True		Tag was found
 * @ret False		Tag was not found
 * @err None
 *
 * Scan through the resource data until we find a particular tag, and
 * read its contents into a buffer.  It is the caller's responsibility
 * to ensure that #buf is large enough to contain a tag of the
 * requested size.
 *
 */
static int isapnp_find_tag ( uint8_t wanted_tag, uint8_t *buf ) {
	uint8_t tag;
	uint16_t len;

	DBG2 ( "ISAPnP read tag" );
	do {
		tag = isapnp_peek_byte();
		if ( ISAPNP_IS_SMALL_TAG ( tag ) ) {
			len = ISAPNP_SMALL_TAG_LEN ( tag );
			tag = ISAPNP_SMALL_TAG_NAME ( tag );
		} else {
			len = isapnp_peek_byte() + ( isapnp_peek_byte() << 8 );
			tag = ISAPNP_LARGE_TAG_NAME ( tag );
		}
		DBG2 ( " %hhx (%hhx)", tag, len );
		if ( tag == wanted_tag ) {
			isapnp_peek ( buf, len );
			DBG2 ( "\n" );
			return 1;
		} else {
			isapnp_peek ( NULL, len );
		}
	} while ( tag != ISAPNP_TAG_END );
	DBG2 ( "\n" );
	return 0;
}

/**
 * Try isolating ISAPnP cards at the current read port.
 *
 * @ret \>0		Number of ISAPnP cards found
 * @ret 0		There are no ISAPnP cards in the system
 * @ret \<0		A conflict was detected; try a new read port
 * @err None
 *
 * The state diagram on page 18 (PDF page 24) of the PnP ISA spec
 * gives the best overview of what happens here.
 *
 */
static int isapnp_try_isolate ( void ) {
	struct isapnp_identifier identifier;
	unsigned int i, j;
	unsigned int seen_55aa, seen_life;
	unsigned int csn = 0;
	uint16_t data;
	uint8_t byte;

	DBG ( "ISAPnP attempting isolation at read port %hx\n",
	      isapnp_read_port );

	/* Place all cards into the Sleep state, whatever state
	 * they're currently in.
	 */
	isapnp_wait_for_key ();
	isapnp_send_key ();

	/* Reset all assigned CSNs */
	isapnp_reset_csn ();
	isapnp_delay();
	isapnp_delay();
	
	/* Place all cards into the Isolation state */
	isapnp_wait_for_key ();
	isapnp_send_key ();
	isapnp_wake ( 0x00 );
	
	/* Set the read port */
	isapnp_set_read_port ();
	isapnp_delay();

	while ( 1 ) {

		/* All cards that do not have assigned CSNs are
		 * currently in the Isolation state, each time we go
		 * through this loop.
		 */

		/* Initiate serial isolation */
		isapnp_serialisolation ();
		isapnp_delay();

		/* Read identifier serially via the ISAPnP read port. */
		memset ( &identifier, 0, sizeof ( identifier ) );
		seen_55aa = seen_life = 0;
		for ( i = 0 ; i < 9 ; i++ ) {
			byte = 0;
			for ( j = 0 ; j < 8 ; j++ ) {
				data = isapnp_read_data ();
				isapnp_delay();
				data = ( data << 8 ) | isapnp_read_data ();
				isapnp_delay();
				byte >>= 1;
				if (  data != 0xffff ) {
					seen_life++;
					if ( data == 0x55aa ) {
						byte |= 0x80;
						seen_55aa++;
					}
				}
			}
			( (char *) &identifier )[i] = byte;
		}

		/* If we didn't see any 55aa patterns, stop here */
		if ( ! seen_55aa ) {
			if ( csn ) {
				DBG ( "ISAPnP found no more cards\n" );
			} else {
				if ( seen_life ) {
					DBG ( "ISAPnP saw life but no cards, "
					      "trying new read port\n" );
					csn = -1;
				} else {
					DBG ( "ISAPnP saw no signs of life, "
					      "abandoning isolation\n" );
				}
			}
			break;
		}

		/* If the checksum was invalid stop here */
		if ( identifier.checksum != isapnp_checksum ( &identifier) ) {
			DBG ( "ISAPnP found malformed card "
			      ISAPNP_CARD_ID_FMT "\n  with checksum %hhx "
			      "(should be %hhx), trying new read port\n",
			      ISAPNP_CARD_ID_DATA ( &identifier ),
			      identifier.checksum,
			      isapnp_checksum ( &identifier) );
			csn = -1;
			break;
		}

		/* Give the device a CSN */
		csn++;
		DBG ( "ISAPnP found card " ISAPNP_CARD_ID_FMT
		      ", assigning CSN %hhx\n",
		      ISAPNP_CARD_ID_DATA ( &identifier ), csn );
		
		isapnp_write_csn ( csn );
		isapnp_delay();

		/* Send this card back to Sleep and force all cards
		 * without a CSN into Isolation state
		 */
		isapnp_wake ( 0x00 );
		isapnp_delay();
	}

	/* Place all cards in Wait for Key state */
	isapnp_wait_for_key ();

	/* Return number of cards found */
	if ( csn > 0 ) {
		DBG ( "ISAPnP found %d cards at read port %hx\n",
		      csn, isapnp_read_port );
	}
	return csn;
}

/**
 * Find a valid read port and isolate all ISAPnP cards.
 *
 */
static void isapnp_isolate ( void ) {
	for ( isapnp_read_port = ISAPNP_READ_PORT_MIN ;
	      isapnp_read_port <= ISAPNP_READ_PORT_MAX ;
	      isapnp_read_port += ISAPNP_READ_PORT_STEP ) {
		/* Avoid problematic locations such as the NE2000
		 * probe space
		 */
		if ( ( isapnp_read_port >= 0x280 ) &&
		     ( isapnp_read_port <= 0x380 ) )
			continue;
		
		/* If we detect any ISAPnP cards at this location, stop */
		if ( isapnp_try_isolate () >= 0 )
			return;
	}
}

/**
 * Increment a #bus_loc structure to the next possible ISAPnP
 * location.
 *
 * @v bus_loc		Bus location
 * @ret True		#bus_loc contains a valid ISAPnP location
 * @ret False		There are no more valid ISAPnP locations
 * @err None
 *
 * If there are no more valid locations, the #bus_loc structure will
 * be zeroed.
 *
 */
static int isapnp_next_location ( struct bus_loc *bus_loc ) {
	struct isapnp_loc *isapnp_loc = ( struct isapnp_loc * ) bus_loc;
	
	/*
	 * Ensure that there is sufficient space in the shared bus
	 * structures for a struct isapnp_loc and a struct isapnp_dev,
	 * as mandated by bus.h.
	 *
	 */
	BUS_LOC_CHECK ( struct isapnp_loc );
	BUS_DEV_CHECK ( struct isapnp_device );

	return ( ++isapnp_loc->logdev ? 1 : ++isapnp_loc->csn );
}

/**
 * Fill in parameters for an ISAPnP device based on CSN.
 *
 * @v bus_dev		Bus device to be filled in
 * @v bus_loc		Bus location as filled in by isapnp_next_location()
 * @ret True		A device is present at this location
 * @ret False		No device is present at this location
 * @err None
 *
 */
static int isapnp_fill_device ( struct bus_dev *bus_dev,
				struct bus_loc *bus_loc ) {
	struct isapnp_device *isapnp = ( struct isapnp_device * ) bus_dev;
	struct isapnp_loc *isapnp_loc = ( struct isapnp_loc * ) bus_loc;
	unsigned int i;
	struct isapnp_identifier identifier;
	struct isapnp_logdevid logdevid;
	static struct {
		uint8_t csn;
		uint8_t first_nonexistent_logdev;
	} cache = { 0, 0 };

	/* Copy CSN and logdev to isapnp_device, set default values */
	isapnp->csn = isapnp_loc->csn;
	isapnp->logdev = isapnp_loc->logdev;
	isapnp->name = "?";

	/* CSN 0 is never valid, but may be passed in */
	if ( ! isapnp->csn )
		return 0;

	/* Check to see if we are already past the maximum CSN */
	if ( isapnp->csn > isapnp_max_csn )
		return 0;

	/* Check cache to see if we are already past the highest
	 * logical device of this CSN
	 */
	if ( ( isapnp->csn == cache.csn ) &&
	     ( isapnp->logdev >= cache.first_nonexistent_logdev ) )
		return 0;

	/* Perform isolation if it hasn't yet been done */
	if ( ! isapnp_read_port )
		isapnp_isolate();

	/* Wake the card */
	isapnp_wait_for_key ();
	isapnp_send_key ();
	isapnp_wake ( isapnp->csn );

	/* Read the card identifier */
	isapnp_peek ( ( char * ) &identifier, sizeof ( identifier ) );

	/* Need to return 0 if no device exists at this CSN */
	if ( identifier.vendor_id & 0x80 ) {
		isapnp_max_csn = isapnp->csn - 1;
		return 0;
	}

	/* Find the Logical Device ID tag corresponding to this device */
	for ( i = 0 ; i <= isapnp->logdev ; i++ ) {
		if ( ! isapnp_find_tag ( ISAPNP_TAG_LOGDEVID,
					 ( char * ) &logdevid ) ) {
			/* No tag for this device */
			if ( isapnp->logdev == 0 ) {
				DBG ( "ISAPnP found no device %hhx.0 on card "
				      ISAPNP_CARD_ID_FMT "\n", isapnp->csn,
				      ISAPNP_CARD_ID_DATA ( &identifier ) );
			}
			cache.csn = isapnp->csn;
			cache.first_nonexistent_logdev = isapnp->logdev;
			return 0;
		}
	}

	/* Read information from logdevid structure */
	isapnp->vendor_id = logdevid.vendor_id;
	isapnp->prod_id = logdevid.prod_id;

	/* Select the logical device */
	isapnp_logicaldevice ( isapnp->logdev );

	/* Read the current ioaddr and irqno */
	isapnp->ioaddr = isapnp_read_iobase ( 0 );
	isapnp->irqno = isapnp_read_irqno ( 0 );

	/* Return all cards to Wait for Key state */
	isapnp_wait_for_key ();

	DBG ( "ISAPnP found device %hhx.%hhx " ISAPNP_DEV_ID_FMT
	      ", base %hx irq %d\n", isapnp->csn, isapnp->logdev,
	      ISAPNP_DEV_ID_DATA ( isapnp ), isapnp->ioaddr, isapnp->irqno );
	DBG ( "  on card " ISAPNP_CARD_ID_FMT "\n",
	      ISAPNP_CARD_ID_DATA ( &identifier ) );

	return 1;
}

/**
 * Test whether or not a driver is capable of driving the device.
 *
 * @v bus_dev		Bus device as filled in by isapnp_fill_device()
 * @v device_driver	Device driver
 * @ret True		Driver is capable of driving this device
 * @ret False		Driver is not capable of driving this device
 * @err None
 *
 */
static int isapnp_check_driver ( struct bus_dev *bus_dev,
				 struct device_driver *device_driver ) {
	struct isapnp_device *isapnp = ( struct isapnp_device * ) bus_dev;
	struct isapnp_driver *driver
		= ( struct isapnp_driver * ) device_driver->bus_driver_info;
	unsigned int i;

	/* Compare against driver's ID list */
	for ( i = 0 ; i < driver->id_count ; i++ ) {
		struct isapnp_id *id = &driver->ids[i];
		
		if ( ( isapnp->vendor_id == id->vendor_id ) &&
		     ( ISA_PROD_ID ( isapnp->prod_id ) ==
		       ISA_PROD_ID ( id->prod_id ) ) ) {
			DBG ( "ISAPnP found ID %hx:%hx (\"%s\") (device %s) "
			      "matching driver %s\n",
			      isapnp->vendor_id, isapnp->prod_id,
			      isa_id_string( isapnp->vendor_id,
					     isapnp->prod_id ),
			      id->name, device_driver->name );
			isapnp->name = id->name;
			return 1;
		}
	}

	return 0;
}

/**
 * Describe an ISAPnP device.
 *
 * @v bus_dev		Bus device as filled in by isapnp_fill_device()
 * @ret string		Printable string describing the device
 * @err None
 *
 * The string returned by isapnp_describe_device() is valid only until
 * the next call to isapnp_describe_device().
 *
 */
static char * isapnp_describe_device ( struct bus_dev *bus_dev ) {
	struct isapnp_device *isapnp = ( struct isapnp_device * ) bus_dev;
	static char isapnp_description[] = "ISAPnP 00:00";

	sprintf ( isapnp_description + 7, "%hhx:%hhx",
		  isapnp->csn, isapnp->logdev );
	return isapnp_description;
}

/**
 * Name an ISAPnP device.
 *
 * @v bus_dev		Bus device as filled in by isapnp_fill_device()
 * @ret string		Printable string naming the device
 * @err None
 *
 * The string returned by isapnp_name_device() is valid only until the
 * next call to isapnp_name_device().
 *
 */
static const char * isapnp_name_device ( struct bus_dev *bus_dev ) {
	struct isapnp_device *isapnp = ( struct isapnp_device * ) bus_dev;
	
	return isapnp->name;
}

/*
 * ISAPnP bus operations table
 *
 */
struct bus_driver isapnp_driver __bus_driver = {
	.name			= "ISAPnP",
	.next_location		= isapnp_next_location,
	.fill_device		= isapnp_fill_device,
	.check_driver		= isapnp_check_driver,
	.describe_device	= isapnp_describe_device,
	.name_device		= isapnp_name_device,
};

/**
 * Activate or deactivate an ISAPnP device.
 *
 * @v isapnp		ISAPnP device
 * @v activation	True to enable, False to disable the device
 * @ret None
 * @err None
 *
 * This routine simply activates the device in its current
 * configuration, or deactivates the device.  It does not attempt any
 * kind of resource arbitration.
 *
 */
void isapnp_device_activation ( struct isapnp_device *isapnp,
				int activation ) {
	/* Wake the card and select the logical device */
	isapnp_wait_for_key ();
	isapnp_send_key ();
	isapnp_wake ( isapnp->csn );
	isapnp_logicaldevice ( isapnp->logdev );

	/* Activate/deactivate the logical device */
	isapnp_activate ( activation );
	isapnp_delay();

	/* Return all cards to Wait for Key state */
	isapnp_wait_for_key ();

	DBG ( "ISAPnP %s device %hhx.%hhx\n",
	      ( activation ? "activated" : "deactivated" ),
	      isapnp->csn, isapnp->logdev );
}

/**
 * Fill in a nic structure.
 *
 * @v nic		NIC structure to be filled in
 * @v isapnp		ISAPnP device
 * @ret None
 * @err None
 *
 * This fills in generic NIC parameters (e.g. I/O address and IRQ
 * number) that can be determined directly from the ISAPnP device,
 * without any driver-specific knowledge.
 *
 */
void isapnp_fill_nic ( struct nic *nic, struct isapnp_device *isapnp ) {

	/* Fill in ioaddr and irqno */
	nic->ioaddr = isapnp->ioaddr;
	nic->irqno = isapnp->irqno;

	/* Fill in DHCP device ID structure */
	nic->dhcp_dev_id.bus_type = ISA_BUS_TYPE;
	nic->dhcp_dev_id.vendor_id = htons ( isapnp->vendor_id );
	nic->dhcp_dev_id.device_id = htons ( isapnp->prod_id );
}

