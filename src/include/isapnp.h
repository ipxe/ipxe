/**************************************************************************
*
*    isapnp.h -- Etherboot isapnp support for the 3Com 3c515
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
*		Copyright (C) 2001  P.J.H.Fox (fox@roestock.demon.co.uk)
*
*
*
*    REVISION HISTORY:
*    ================
*        Version 0.1 April 26, 2002 	TJL
*	 Version 0.2 01/08/2003			TJL Renamed from 3c515_isapnp.h
*
*
*    Generalised into an ISAPnP bus that can be used by more than just
*    the 3c515 by Michael Brown <mbrown@fensystems.co.uk>
*
***************************************************************************/

#ifndef ISAPNP_H
#define ISAPNP_H

#include "isa_ids.h"
#include "dev.h"

/*
 * ISAPnP constants
 *
 */

/* Port addresses */
#define ISAPNP_ADDRESS		0x279
#define ISAPNP_WRITE_DATA	0xa79
#define ISAPNP_READ_PORT_MIN	0x213	/* ISAPnP spec says 0x203, but
					 * Linux ISAPnP starts at
					 * 0x213 with no explanatory
					 * comment.  0x203 probably
					 * clashes with something. */
#define ISAPNP_READ_PORT_MAX	0x3ff
#define ISAPNP_READ_PORT_STEP	0x08	/* Can be any multiple of 4 */

/* Registers */
#define ISAPNP_READPORT			0x00
#define ISAPNP_SERIALISOLATION 		0x01
#define ISAPNP_CONFIGCONTROL		0x02
#define ISAPNP_WAKE			0x03
#define ISAPNP_RESOURCEDATA		0x04
#define ISAPNP_STATUS          		0x05
#define ISAPNP_CARDSELECTNUMBER		0x06
#define ISAPNP_LOGICALDEVICENUMBER	0x07
#define ISAPNP_ACTIVATE			0x30
#define ISAPNP_IORANGECHECK		0x31

/* Bits in the CONFIGCONTROL register */
#define ISAPNP_CONFIG_RESET		( 1 << 0 )
#define ISAPNP_CONFIG_WAIT_FOR_KEY	( 1 << 1 )
#define ISAPNP_CONFIG_RESET_CSN		( 1 << 2 )
#define ISAPNP_CONFIG_RESET_DRV		( ISAPNP_CONFIG_RESET | 	\
					  ISAPNP_CONFIG_WAIT_FOR_KEY |	\
					  ISAPNP_CONFIG_RESET_CSN )

/* The LFSR used for the initiation key and for checksumming */
#define ISAPNP_LFSR_SEED	0x6a

/* Short  Tags */
#define PnPVerNo_TAG		0x01
#define LogDevId_TAG		0x02
#define CompatDevId_TAG		0x03
#define IRQ_TAG			0x04
#define DMA_TAG			0x05
#define StartDep_TAG		0x06
#define EndDep_TAG		0x07
#define IOport_TAG		0x08
#define FixedIO_TAG		0x09
#define RsvdShortA_TAG		0x0A
#define RsvdShortB_TAG		0x0B
#define RsvdShortC_TAG		0x0C
#define RsvdShortD_TAG		0x0D
#define VendorShort_TAG		0x0E
#define End_TAG			0x0F
/* Long  Tags */
#define MemRange_TAG		0x81
#define ANSIstr_TAG		0x82
#define UNICODEstr_TAG		0x83
#define VendorLong_TAG		0x84
#define Mem32Range_TAG		0x85
#define FixedMem32Range_TAG	0x86
#define RsvdLong0_TAG		0xF0
#define RsvdLong1_TAG		0xF1
#define RsvdLong2_TAG		0xF2
#define RsvdLong3_TAG		0xF3
#define RsvdLong4_TAG		0xF4
#define RsvdLong5_TAG		0xF5
#define RsvdLong6_TAG		0xF6
#define RsvdLong7_TAG		0xF7
#define RsvdLong8_TAG		0xF8
#define RsvdLong9_TAG		0xF9
#define RsvdLongA_TAG		0xFA
#define RsvdLongB_TAG		0xFB
#define RsvdLongC_TAG		0xFC
#define RsvdLongD_TAG		0xFD
#define RsvdLongE_TAG		0xFE
#define RsvdLongF_TAG		0xFF
#define NewBoard_PSEUDOTAG	0x100

/*
 * An ISAPnP serial identifier
 *
 */
union isapnp_identifier {
	char bytes[9];
	struct {
		uint16_t vendor_id;
		uint16_t product_id;
		uint32_t serial;
		uint8_t checksum;
	} __attribute__ (( packed ));
} __attribute__ (( packed ));

/*
 * A physical ISAPnP device
 *
 */
struct isapnp_device {
	char *magic; /* must be first */
	const char *name;
	unsigned char csn;
	uint16_t vendor_id;
	uint16_t prod_id;
	int already_tried;
};

/*
 * An individual ISAPnP device identified by ID
 *
 */
struct isapnp_id {
        const char *name;
	uint16_t vendor_id, prod_id;
};

/*
 * An ISAPnP driver, with a device ID (struct isapnp_id) table.
 *
 */
struct isapnp_driver {
	const char *name;
	struct isapnp_id *ids;
	unsigned int id_count;
};

/*
 * Define an ISAPnP driver
 *
 */
#define ISAPNP_DRIVER( driver_name, isapnp_ids ) {			\
	.name = driver_name,						\
	.ids = isapnp_ids,						\
	.id_count = sizeof ( isapnp_ids ) / sizeof ( isapnp_ids[0] ),	\
}

/*
 * Functions in isapnp.c
 *
 */
extern int find_isapnp_device ( struct isapnp_device *isapnp,
				struct isapnp_driver *driver );
extern int find_isapnp_boot_device ( struct dev *dev,
				     struct isapnp_driver *driver );

#endif /* ISAPNP_H */
