/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <timer.h>
#include <gpxe/nvs/threewire.h>

/** @file
 *
 * Three-wire serial interface
 *
 */

/**
 * Read from a three-wire device
 *
 * @v three	Three-wire interface
 * @v address	Address
 * @ret data	Data
 */
unsigned long threewire_read ( struct threewire *three,
			       unsigned long address ) {
	struct threewire_operations *ops = three->ops;
	unsigned long command;
	unsigned long data;
	int i;

	ops->setcs ( three, 1 );
	
	/* Send command and address */
	command = threewire_cmd_read ( three, address );
	for ( i = ( threewire_cmd_len ( three ) - 1 ) ; i >= 0 ; i-- ) {
		ops->setdi ( three, ( command >> i ) & 0x1 );
		udelay ( three->udelay );
		ops->setsk ( three, 1 );
		udelay ( three->udelay );
		ops->setsk ( three, 0 );
	}

	/* Read back data */
	data = 0;
	for ( i = three->datasize ; i ; i-- ) {
		udelay ( three->udelay );
		ops->setsk ( three, 1 );
		udelay ( three->udelay );
		data <<= 1;
		data |= ops->getdo ( three );
		ops->setsk ( three, 0 );
	}

	ops->setcs ( three, 0 );

	return data;
}
