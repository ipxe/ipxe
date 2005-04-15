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

#include "etherboot.h"
#include "timer.h"
#include "io.h"
#include "isapnp.h"

/*
 * Ensure that there is sufficient space in the shared dev_bus
 * structure for a struct isapnp_device.
 *
 */
DEV_BUS( struct isapnp_device, isapnp_dev );
static char isapnp_magic[0]; /* guaranteed unique symbol */

/*
 * We can have only one ISAPnP bus in a system.  Once the read port is
 * known and all cards have been allocated CSNs, there's nothing to be
 * gained by re-scanning for cards.
 *
 * However, we shouldn't make scanning the ISAPnP bus an INIT_FN(),
 * because even ISAPnP probing can still screw up other devices on the
 * ISA bus.  We therefore probe only when we are first asked to find
 * an ISAPnP device.
 *
 */
static uint16_t isapnp_read_port;
static uint16_t isapnp_max_csn;

/*
 * ISAPnP utility functions
 *
 */

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

static inline void isapnp_set_read_port ( void ) {
	isapnp_write_byte ( ISAPNP_READPORT, isapnp_read_port >> 2 );
}

static inline void isapnp_serialisolation ( void ) {
	isapnp_write_address ( ISAPNP_SERIALISOLATION );
}

static inline void isapnp_wait_for_key ( void ) {
	isapnp_write_byte ( ISAPNP_CONFIGCONTROL, ISAPNP_CONFIG_WAIT_FOR_KEY );
}

static inline void isapnp_reset_csn ( void ) {
	isapnp_write_byte ( ISAPNP_CONFIGCONTROL, ISAPNP_CONFIG_RESET_CSN );
}

static inline void isapnp_wake ( uint8_t csn ) {
	isapnp_write_byte ( ISAPNP_WAKE, csn );
}

static inline uint8_t isapnp_read_resourcedata ( void ) {
	return isapnp_read_byte ( ISAPNP_RESOURCEDATA );
}

static inline uint8_t isapnp_read_status ( void ) {
	return isapnp_read_byte ( ISAPNP_STATUS );
}

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

/*
 * The linear feedback shift register as described in Appendix B of
 * the PnP ISA spec.  The hardware implementation uses eight D-type
 * latches and two XOR gates.  I think this is probably the smallest
 * possible implementation in software.  Six instructions when input_bit
 * is a constant 0 (for isapnp_send_key).  :)
 *
 */
static inline uint8_t isapnp_lfsr_next ( uint8_t lfsr, int input_bit ) {
	register uint8_t lfsr_next;

	lfsr_next = lfsr >> 1;
	lfsr_next |= ( ( ( lfsr ^ lfsr_next ) ^ input_bit ) ) << 7;
	return lfsr_next;
}

/*
 * Send the ISAPnP initiation key
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

/*
 *  Compute ISAPnP identifier checksum
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

/*
 * Read n bytes of resource data from the current location.  If buf is
 * NULL, discard data.
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

/*
 * Scan through the resource data until we find a particular tag, and
 * read its contents into a buffer.
 *
 * It is the caller's responsibility to ensure that buf is large
 * enough to contain a tag of the requested size.
 *
 */
static int isapnp_find_tag ( uint8_t wanted_tag, uint8_t *buf ) {
	uint8_t tag;
	uint16_t len;

	do {
		tag = isapnp_peek_byte();
		if ( ISAPNP_IS_SMALL_TAG ( tag ) ) {
			len = ISAPNP_SMALL_TAG_LEN ( tag );
			tag = ISAPNP_SMALL_TAG_NAME ( tag );
		} else {
			len = isapnp_peek_byte() + ( isapnp_peek_byte() << 8 );
			tag = ISAPNP_LARGE_TAG_NAME ( tag );
		}
		DBG ( "ISAPnP read tag %hhx len %hhx\n", tag, len );
		if ( tag == wanted_tag ) {
			isapnp_peek ( buf, len );
			return 1;
		} else {
			isapnp_peek ( NULL, len );
		}
	} while ( tag != ISAPNP_TAG_END );
	return 0;
}

/*
 * Try isolating ISAPnP cards at the current read port.  Return the
 * number of ISAPnP cards found.
 *
 * The state diagram on page 18 (PDF page 24) of the PnP ISA spec
 * gives the best overview of what happens here.
 *
 */
static int isapnp_try_isolate ( void ) {
	struct isapnp_identifier identifier;
	int i, j, seen55aa;
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
	isapnp_max_csn = 0;
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
		seen55aa = 0;
		for ( i = 0 ; i < 9 ; i++ ) {
			byte = 0;
			for ( j = 0 ; j < 8 ; j++ ) {
				data = isapnp_read_data ();
				isapnp_delay();
				data = ( data << 8 ) | isapnp_read_data ();
				isapnp_delay();
				if ( data == 0x55aa ) {
					byte |= 1;
				}
				byte <<= 1;
			}
			( (char *) &identifier )[i] = byte;
			if ( byte ) {
				seen55aa = 1;
			}
		}
				
		/* If we didn't see a valid ISAPnP device, stop here */
		if ( ( ! seen55aa ) ||
		     ( identifier.checksum != isapnp_checksum (&identifier) ) )
			break;

		/* Give the device a CSN */
		isapnp_max_csn++;
		DBG ( "ISAPnP isolation found card %hhx ID %hx:%hx (\"%s\") "
		      "serial %x checksum %hhx, assigning CSN %hhx\n",
		      identifier.vendor_id, identifier.prod_id,
		      isa_id_string ( identifier.vendor_id,
				      identifier.prod_id ),
		      identifier.serial, identifier.checksum, isapnp_max_csn );
		
		isapnp_write_csn ( isapnp_max_csn );
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
	DBG ( "ISAPnP found %d cards at read port %hx\n",
	      isapnp_max_csn, isapnp_read_port );
	return isapnp_max_csn;
}

/*
 * Isolate all ISAPnP cards, locating a valid read port in the process.
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
		if ( isapnp_try_isolate () )
			return;
	}
}

/*
 * Fill in parameters for an ISAPnP device based on CSN
 *
 * Return 1 if device present, 0 otherwise
 *
 */
static int fill_isapnp_device ( struct isapnp_device *isapnp ) {
	unsigned int i;
	struct isapnp_logdevid logdevid;
	
	/* Wake the card */
	isapnp_wait_for_key ();
	isapnp_send_key ();
	isapnp_wake ( isapnp->csn );

	/* Skip past the card identifier */
	isapnp_peek ( NULL, sizeof ( struct isapnp_identifier ) );

	/* Find the Logical Device ID tag corresponding to this device */
	for ( i = 0 ; i <= isapnp->logdev ; i++ ) {
		if ( ! isapnp_find_tag ( ISAPNP_TAG_LOGDEVID,
					 ( char * ) &logdevid ) ) {
			/* No tag for this device */
			return 0;
		}
	}

	/* Read information from identifier structure */
	isapnp->vendor_id = logdevid.vendor_id;
	isapnp->prod_id = logdevid.prod_id;

	/* Select the logical device */
	isapnp_logicaldevice ( isapnp->logdev );

	/* Read the current ioaddr and irqno */
	isapnp->ioaddr = isapnp_read_iobase ( 0 );
	isapnp->irqno = isapnp_read_irqno ( 0 );

	/* Return all cards to Wait for Key state */
	isapnp_wait_for_key ();

	DBG ( "ISAPnP found device %hhx.%hhx ID %hx:%hx (\"%s\"), "
	      "base %hx irq %d\n",
	      isapnp->csn, isapnp->logdev, isapnp->vendor_id, isapnp->prod_id,
	      isa_id_string ( isapnp->vendor_id, isapnp->prod_id ),
	      isapnp->ioaddr, isapnp->irqno );

	return 1;
}

/*
 * Find an ISAPnP device matching the specified driver
 *
 */
int find_isapnp_device ( struct isapnp_device *isapnp,
			 struct isapnp_driver *driver ) {
	unsigned int i;

	/* Initialise struct isapnp if it's the first time it's been used. */
	if ( isapnp->magic != isapnp_magic ) {
		memset ( isapnp, 0, sizeof ( *isapnp ) );
		isapnp->magic = isapnp_magic;
		isapnp->csn = 1;
	}

	/* Ensure that all ISAPnP cards have CSNs allocated to them,
	 * if we haven't already done so.
	 */
	if ( ! isapnp_read_port ) {
		isapnp_isolate();
	}

	/* Iterate through all possible ISAPNP CSNs, starting where we
	 * left off.
	 */
	for ( ; isapnp->csn <= isapnp_max_csn ; isapnp->csn++ ) {
		for ( ; isapnp->logdev <= 0xff ; isapnp->logdev++ ) {
			/* If we've already used this device, skip it */
			if ( isapnp->already_tried ) {
				isapnp->already_tried = 0;
				continue;
			}
			
			/* Fill in device parameters */
			if ( ! fill_isapnp_device ( isapnp ) ) {
				/* If fill_isapnp_device fails, assume
				 * that we've reached the last logical
				 * device on this card, and proceed to
				 * the next card.
				 */
				isapnp->logdev = 0;
				break;
			}

			/* Compare against driver's ID list */
			for ( i = 0 ; i < driver->id_count ; i++ ) {
				struct isapnp_id *id = &driver->ids[i];
				
				if ( ( isapnp->vendor_id == id->vendor_id ) &&
				     ( ISA_PROD_ID ( isapnp->prod_id ) ==
				       ISA_PROD_ID ( id->prod_id ) ) ) {
					DBG ( "Device %s (driver %s) "
					      "matches ID %s\n",
					      id->name, driver->name,
					      isa_id_string( isapnp->vendor_id,
							   isapnp->prod_id ) );
					isapnp->name = id->name;
					isapnp->already_tried = 1;
					return 1;
				}
			}
		}
	}

	/* No device found */
	isapnp->csn = 1;
	return 0;
}

/*
 * Find the next ISAPNP device that can be used to boot using the
 * specified driver.
 *
 */
int find_isapnp_boot_device ( struct dev *dev, struct isapnp_driver *driver ) {
	struct isapnp_device *isapnp = ( struct isapnp_device * ) dev->bus;

	if ( ! find_isapnp_device ( isapnp, driver ) )
		return 0;

	dev->name = isapnp->name;
	dev->devid.bus_type = ISA_BUS_TYPE;
	dev->devid.vendor_id = isapnp->vendor_id;
	dev->devid.device_id = isapnp->prod_id;

	return 1;
}

/*
 * Activate or deactivate an ISAPnP device
 *
 * This routine simply activates the device in its current
 * configuration.  It does not attempt any kind of resource
 * arbitration.
 *
 */
void activate_isapnp_device ( struct isapnp_device *isapnp,
			      int activate ) {
	/* Wake the card and select the logical device */
	isapnp_wait_for_key ();
	isapnp_send_key ();
	isapnp_wake ( isapnp->csn );
	isapnp_logicaldevice ( isapnp->logdev );

	/* Activate/deactivate the logical device */
	isapnp_activate ( activate );
	isapnp_delay();

	/* Return all cards to Wait for Key state */
	isapnp_wait_for_key ();

	DBG ( "ISAPnP activated device %hhx.%hhx\n",
	      isapnp->csn, isapnp->logdev );
}
