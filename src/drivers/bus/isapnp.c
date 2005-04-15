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

/*
 * The linear feedback shift register as described in Appendix B of
 * the PnP ISA spec.  The hardware implementation uses eight D-type
 * latches and two XOR gates.  I think this is probably the smallest
 * possible implementation in software.  :)
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

	udelay ( 1000 );
	isapnp_write_address ( 0x00 );
	isapnp_write_address ( 0x00 );

	lfsr = ISAPNP_LFSR_SEED;
	for ( i = 0 ; i < 32 ; i-- ) {
		isapnp_write_address ( lfsr );
		lfsr = isapnp_lfsr_next ( lfsr, 0 );
	}
}

/*
 *  Compute ISAPnP identifier checksum
 *
 */
static uint8_t isapnp_checksum ( union isapnp_identifier *identifier ) {
	int i, j;
	uint8_t lfsr;
	uint8_t byte;

	lfsr = ISAPNP_LFSR_SEED;
	for ( i = 0 ; i < 8 ; i++ ) {
		byte = identifier->bytes[i];
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
		udelay ( 100 );
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
 * Try isolating ISAPnP cards at the current read port.  Return the
 * number of ISAPnP cards found.
 *
 * The state diagram on page 18 (PDF page 24) of the PnP ISA spec
 * gives the best overview of what happens here.
 *
 */
static int isapnp_try_isolate ( void ) {
	union isapnp_identifier identifier;
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
	udelay ( 2000 );
	
	/* Place all cards into the Isolation state */
	isapnp_wait_for_key ();
	isapnp_send_key ();
	isapnp_wake ( 0x00 );
	
	/* Set the read port */
	isapnp_set_read_port ();
	udelay ( 1000 );

	while ( 1 ) {

		/* All cards that do not have assigned CSNs are
		 * currently in the Isolation state, each time we go
		 * through this loop.
		 */

		/* Initiate serial isolation */
		isapnp_serialisolation ();
		udelay ( 1000 );

		/* Read identifier serially via the ISAPnP read port. */
		memset ( &identifier, 0, sizeof ( identifier ) );
		seen55aa = 0;
		for ( i = 0 ; i < 9 ; i++ ) {
			byte = 0;
			for ( j = 0 ; j < 8 ; j++ ) {
				data = isapnp_read_data ();
				udelay ( 1000 );
				data = ( data << 8 ) | isapnp_read_data ();
				udelay ( 1000 );
				if ( data == 0x55aa ) {
					byte |= 1;
				}
				byte <<= 1;
			}
			identifier.bytes[i] = byte;
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
		DBG ( "ISAPnP isolation found device "
		      "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx "
		      "(checksum %hhx), assigning CSN %hhx\n",
		      identifier.bytes[0], identifier.bytes[1],
		      identifier.bytes[2], identifier.bytes[3],
		      identifier.bytes[4], identifier.bytes[5],
		      identifier.bytes[6], identifier.bytes[7],
		      identifier.checksum, isapnp_max_csn );
		
		isapnp_write_csn ( isapnp_max_csn );
		udelay ( 1000 );

		/* Send this card back to Sleep and force all cards
		 * without a CSN into Isolation state
		 */
		isapnp_wake ( 0x00 );
		udelay ( 1000 );
	}

	/* Place all cards in Wait for Key state */
	isapnp_wait_for_key ();

	/* Return number of cards found */
	DBG ( "ISAPnP found %d devices at read port %hx\n", isapnp_read_port );
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
	union isapnp_identifier identifier;
	
	/* Ensure that all ISAPnP cards have CSNs allocated to them,
	 * if we haven't already done so.
	 */
	if ( ! isapnp_read_port ) {
		isapnp_isolate();
	}

	/* Wake the specified CSN */
	isapnp_wait_for_key ();
	isapnp_send_key ();
	isapnp_wake ( isapnp->csn );

	/* Read the identifier and verify the checksum.  Allow
	 * checksum = 0 to cope with cards that just generate the
	 * checksum using the LFSR during serial isolation.
	 */
	isapnp_peek ( identifier.bytes, sizeof ( identifier ) );
	if ( ( identifier.checksum != 0 ) &&
	     ( identifier.checksum != isapnp_checksum ( &identifier ) ) ) {
		DBG ( "ISAPnP invalid checksum on CSN %hhx "
		      "(is %hhx, should be %hhx)\n", isapnp->csn,
		      identifier.checksum, isapnp_checksum ( &identifier ) );
		return 0;
	}

	/* Read information from identifier structure */
	isapnp->vendor_id = identifier.vendor_id;
	isapnp->prod_id = identifier.prod_id;

	/* Return all cards to Wait for Key state */
	isapnp_wait_for_key ();

	DBG ( "ISAPnP found CSN %hhx ID %hx:%hx (\"%s\")\n",
	      isapnp->csn, isapnp->vendor_id, isapnp->prod_id,
	      isa_id_string ( isapnp->vendor_id, isapnp->prod_id ) );

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

	/* Iterate through all possible ISAPNP CSNs, starting where we
	 * left off.
	 */
	for ( ; isapnp->csn <= isapnp_max_csn ; isapnp->csn++ ) {
		/* If we've already used this device, skip it */
		if ( isapnp->already_tried ) {
			isapnp->already_tried = 0;
			continue;
		}

		/* Fill in device parameters */
		if ( ! fill_isapnp_device ( isapnp ) ) {
			continue;
		}

		/* Compare against driver's ID list */
		for ( i = 0 ; i < driver->id_count ; i++ ) {
			struct isapnp_id *id = &driver->ids[i];
			
			if ( ( isapnp->vendor_id == id->vendor_id ) &&
			     ( ISA_PROD_ID ( isapnp->prod_id ) ==
			       ISA_PROD_ID ( id->prod_id ) ) ) {
				DBG ( "Device %s (driver %s) matches ID %s\n",
				      id->name, driver->name,
				      isa_id_string ( isapnp->vendor_id,
						      isapnp->prod_id ) );
				isapnp->name = id->name;
				isapnp->already_tried = 1;
				return 1;
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
 * Activate a logical function on an ISAPnP device
 *
 * This routine simply activates the device in its current
 * configuration.  It does not attempt any kind of resource
 * arbitration.
 *
 */
void activate_isapnp_device ( struct isapnp_device *isapnp,
			      uint8_t logdev ) {
	/* Wake the device */
	isapnp_wait_for_key ();
	isapnp_send_key ();
	isapnp_wake ( isapnp->csn );

	/* Select the specified logical device */
	isapnp_activate ( logdev );
	udelay ( 1000 );
	
	/* Return all cards to Wait for Key state */
	isapnp_wait_for_key ();
}

/*
 * Deactivate a logical function on an ISAPnP device
 *
 */
void deactivate_isapnp_device ( struct isapnp_device *isapnp,
			      uint8_t logdev ) {
	/* Wake the device */
	isapnp_wait_for_key ();
	isapnp_send_key ();
	isapnp_wake ( isapnp->csn );

	/* Select the specified logical device */
	isapnp_deactivate ( logdev );
	udelay ( 1000 );
	
	/* Return all cards to Wait for Key state */
	isapnp_wait_for_key ();
}
