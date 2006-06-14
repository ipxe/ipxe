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

struct spi_interface;

/** A three-wire device */
struct threewire_device {
	/** SPI interface to which device is attached */
	struct spi_interface *spi;
	/** SPI slave number */
	unsigned int slave;
	/** Address size (in bits) */
	unsigned int adrsize;
	/** Data size (in bits) */
	unsigned int datasize;
};

/**
 * Calculate read command for a specified address
 *
 * @v three	Three-wire interface
 * @v address	Address
 * @ret cmd	Command
 */
static inline __attribute__ (( always_inline )) unsigned long
threewire_cmd_read ( struct threewire_device *three, unsigned long address ) {
	return ( ( 0x6 << three->adrsize ) | address );
}

/**
 * Calculate command length
 *
 * @v three	Three-wire interface
 * @ret len	Command length, in bits
 */
static inline __attribute__ (( always_inline )) unsigned int
threewire_cmd_len ( struct threewire_device *three ) {
	return ( three->adrsize + 3 );
}

/* Constants for some standard parts */
#define AT93C46_ORG8_ADRSIZE	7
#define AT93C46_ORG8_DATASIZE	8
#define AT93C46_ORG16_ADRSIZE	6
#define AT93C46_ORG16_DATASIZE	16
#define AT93C46_UDELAY		1
#define AT93C56_ORG8_ADRSIZE	9
#define AT93C56_ORG8_DATASIZE	8
#define AT93C56_ORG16_ADRSIZE	8
#define AT93C56_ORG16_DATASIZE	16
#define AT93C56_UDELAY		1

extern unsigned long threewire_read ( struct threewire_device *three,
				      unsigned long address );

#endif /* _GPXE_THREEWIRE_H */
