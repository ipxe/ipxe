#ifndef _GPXE_THREEWIRE_H
#define _GPXE_THREEWIRE_H

/** @file
 *
 * Three-wire serial interface
 *
 * The Atmel three-wire interface is a subset of the (newer) SPI
 * interface, and is implemented here as a layer on top of the SPI
 * support.
 */

#include <gpxe/spi.h>

/**
 * @defgroup tcmds Three-wire commands
 * @{
 */

/** Read data from memory array */
#define THREEWIRE_READ 0x6

/** @} */

/**
 * @defgroup spidevs SPI device types
 * @{
 */

/** Atmel AT93C46 serial EEPROM
 *
 * @v org	Word size (8 or 16)
 */
#define AT93C46( org ) {				\
	.word_len = (org),				\
	.size = ( 1024 / (org) ),			\
	.block_size = 1,				\
	.command_len = 3,				\
	.address_len = ( ( (org) == 8 ) ? 7 : 6 ),	\
	.read = threewire_read,				\
	}

/** Atmel AT93C56 serial EEPROM
 *
 * @v org	Word size (8 or 16)
 */
#define AT93C56( org ) {				\
	.word_len = (org),				\
	.size = ( 2048 / (org) ),			\
	.block_size = 1,				\
	.command_len = 3,				\
	.address_len = ( ( (org) == 8 ) ? 9 : 8 ),	\
	.read = threewire_read,				\
	}

/** @} */

extern int threewire_read ( struct spi_device *device, unsigned int address,
			    void *data, size_t len );

#endif /* _GPXE_THREEWIRE_H */
