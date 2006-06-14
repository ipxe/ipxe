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

#include <stddef.h>
#include <byteswap.h>
#include <gpxe/spi.h>
#include <gpxe/threewire.h>

/** @file
 *
 * Three-wire serial devices
 *
 */

/**
 * Read from a three-wire device
 *
 * @v three	Three-wire device
 * @v address	Address
 * @ret data	Data
 */
unsigned long threewire_read ( struct threewire_device *three,
			       unsigned long address ) {
	struct spi_interface *spi = three->spi;
	uint32_t data;

	/* Activate chip select line */
	spi->select_slave ( spi, three->slave );

	/* Send command and address */
	data = cpu_to_le32 ( threewire_cmd_read ( three, address ) );
	spi->transfer ( spi, &data, NULL, threewire_cmd_len ( three ) );
	
	/* Read back data */
	data = 0;
	spi->transfer ( spi, NULL, &data, three->datasize );

	/* Deactivate chip select line */
	spi->deselect_slave ( spi );

	return le32_to_cpu ( data );;
}
