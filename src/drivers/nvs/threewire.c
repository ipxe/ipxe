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
#include <assert.h>
#include <gpxe/threewire.h>

/** @file
 *
 * Three-wire serial devices
 *
 */

/** Read data from three-wire device
 *
 * @v device		SPI device
 * @v address		Address from which to read
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
int threewire_read ( struct spi_device *device, unsigned int address,
		     void *data, size_t len ) {
	struct spi_bus *bus = device->bus;

	assert ( bus->mode == SPI_MODE_THREEWIRE );

	DBG ( "3wire %p reading %d bytes at %04x\n", device, len, address );

	return bus->rw ( bus, device, THREEWIRE_READ, address,
			 NULL, data, len );
}
