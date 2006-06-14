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
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <timer.h>
#include <gpxe/bitbash.h>
#include <gpxe/spi.h>

/** @file
 *
 * SPI bit-bashing interface
 *
 */

/** Delay between SCLK changes and around SS changes */
static void spi_delay ( void ) {
	udelay ( SPI_UDELAY );
}

/**
 * Select/deselect slave
 *
 * @v spi		SPI bit-bashing interface
 * @v slave		Slave number
 * @v state		Slave select state
 *
 * @c state must be set to zero to select the specified slave, or to
 * @c SPI_MODE_SSPOL to deselect the slave.
 */
static void spi_bit_set_slave_select ( struct spi_bit_basher *spibit,
				       unsigned int slave,
				       unsigned int state ) {
	struct bit_basher *basher = &spibit->basher;

	state ^= ( spibit->spi.mode & SPI_MODE_SSPOL );
	DBG ( "Setting slave %d select %s\n", slave,
	      ( state ? "high" : "low" ) );

	spi_delay();
	write_bit ( basher, SPI_BIT_SS ( slave ), state );
	spi_delay();
}

/**
 * Select slave
 *
 * @v spi		SPI interface
 * @v slave		Slave number
 */
static void spi_bit_select_slave ( struct spi_interface *spi,
				   unsigned int slave ) {
	struct spi_bit_basher *spibit
		= container_of ( spi, struct spi_bit_basher, spi );

	spibit->slave = slave;
	spi_bit_set_slave_select ( spibit, slave, 0 );
}

/**
 * Deselect slave
 *
 * @v spi		SPI interface
 */
static void spi_bit_deselect_slave ( struct spi_interface *spi ) {
	struct spi_bit_basher *spibit
		= container_of ( spi, struct spi_bit_basher, spi );

	spi_bit_set_slave_select ( spibit, spibit->slave, SPI_MODE_SSPOL );
}

/**
 * Transfer bits over SPI bit-bashing interface
 *
 * @v spi		SPI interface
 * @v data_out		TX data buffer (or NULL)
 * @v data_in		RX data buffer (or NULL)
 * @v len		Length of transfer (in @b bits)
 *
 * This issues @c len clock cycles on the SPI bus, shifting out data
 * from the @c data_out buffer to the MOSI line and shifting in data
 * from the MISO line to the @c data_in buffer.  If @c data_out is
 * NULL, then the data sent will be all zeroes.  If @c data_in is
 * NULL, then the incoming data will be discarded.
 */
static void spi_bit_transfer ( struct spi_interface *spi, const void *data_out,
			       void *data_in, unsigned int len ) {
	struct spi_bit_basher *spibit
		= container_of ( spi, struct spi_bit_basher, spi );
	struct bit_basher *basher = &spibit->basher;
	unsigned int sclk = ( ( spi->mode & SPI_MODE_CPOL ) ? 1 : 0 );
	unsigned int cpha = ( ( spi->mode & SPI_MODE_CPHA ) ? 1 : 0 );
	unsigned int offset;
	unsigned int mask;
	unsigned int bit;
	int step;

	DBG ( "Transferring %d bits in mode %x\n", len, spi->mode );

	for ( step = ( ( len * 2 ) - 1 ) ; step >= 0 ; step-- ) {
		/* Calculate byte offset within data and bit mask */
		offset = ( step / 16 );
		mask = ( 1 << ( ( step % 16 ) / 2 ) );
		
		/* Shift data in or out */
		if ( sclk == cpha ) {
			const uint8_t *byte;

			/* Shift data out */
			if ( data_out ) {
				byte = ( data_out + offset );
				bit = ( *byte & mask );
			} else {
				bit = 0;
			}
			write_bit ( basher, SPI_BIT_MOSI, bit );
		} else {
			uint8_t *byte;

			/* Shift data in */
			bit = read_bit ( basher, SPI_BIT_MISO );
			if ( data_in ) {
				byte = ( data_in + offset );
				*byte &= ~mask;
				*byte |= ( bit & mask );
			}
		}

		/* Toggle clock line */
		spi_delay();
		sclk = ~sclk;
		write_bit ( basher, SPI_BIT_SCLK, sclk );
	}
}

/**
 * Initialise SPI bit-bashing interface
 *
 * @v spibit		SPI bit-bashing interface
 */
void init_spi_bit_basher ( struct spi_bit_basher *spibit ) {
	assert ( &spibit->basher.read != NULL );
	assert ( &spibit->basher.write != NULL );
	spibit->spi.select_slave = spi_bit_select_slave;
	spibit->spi.deselect_slave = spi_bit_deselect_slave;
	spibit->spi.transfer = spi_bit_transfer;
}
