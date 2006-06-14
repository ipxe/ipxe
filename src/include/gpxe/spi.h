#ifndef _GPXE_SPI_H
#define _GPXE_SPI_H

/** @file
 *
 * SPI interface
 *
 */

#include <gpxe/bitbash.h>

/** An SPI interface */
struct spi_interface {
	/** SPI interface mode
	 *
	 * This is the bitwise OR of zero or more of @c SPI_MODE_CPHA
	 * and @c SPI_MODE_CPOL.  It is also the number conventionally
	 * used to describe the SPI interface mode.  For example, SPI
	 * mode 1 is the mode in which CPOL=0 and CPHA=1, which
	 * therefore corresponds to a mode value of (0|SPI_MODE_CPHA)
	 * which, happily, equals 1.
	 */
	unsigned int mode;
	/**
	 * Select slave
	 *
	 * @v spi		SPI interface
	 * @v slave		Slave number
	 */
	void ( * select_slave ) ( struct spi_interface *spi,
				  unsigned int slave );
	/**
	 * Deselect slave
	 *
	 * @v spi		SPI interface
	 */
	void ( * deselect_slave ) ( struct spi_interface *spi );
	/**
	 * Transfer bits over SPI bit-bashing interface
	 *
	 * @v spi		SPI interface
	 * @v data_out		TX data buffer (or NULL)
	 * @v data_in		RX data buffer (or NULL)
	 * @v len		Length of transfer (in @b bits)
	 */
	void ( * transfer ) ( struct spi_interface *spi, const void *data_out,
			      void *data_in, unsigned int len );
};

/** Clock phase (CPHA) mode bit
 *
 * Phase 0 is sample on rising edge, shift data on falling edge.
 *
 * Phase 1 is shift data on rising edge, sample data on falling edge.
 */
#define SPI_MODE_CPHA 0x01

/** Clock polarity (CPOL) mode bit
 *
 * This bit reflects the idle state of the clock line (SCLK).
 */
#define SPI_MODE_CPOL 0x02

/** Slave select polarity mode bit
 *
 * This bit reflects that active state of the slave select lines.  It
 * is not part of the normal SPI mode number (which covers only @c
 * SPI_MODE_CPOL and @c SPI_MODE_CPHA), but is included here for
 * convenience.
 */
#define SPI_MODE_SSPOL 0x10

/** Microwire-compatible mode
 *
 * This is SPI mode 1 (i.e. CPOL=0, CPHA=1), and is compatible with
 * the original Microwire protocol.
 */
#define SPI_MODE_MICROWIRE 1

/** Microwire/Plus-compatible mode
 *
 * This is SPI mode 0 (i.e. CPOL=0, CPHA=0), and is compatible with
 * the Microwire/Plus protocol
 */
#define SPI_MODE_MICROWIRE_PLUS 0

/** Threewire-compatible mode
 *
 * This mode is compatible with Atmel's series of "three-wire"
 * interfaces.
 */
#define SPI_MODE_THREEWIRE ( SPI_MODE_MICROWIRE_PLUS | SPI_MODE_SSPOL )

/** A bit-bashing SPI interface */
struct spi_bit_basher {
	/** SPI interface */
	struct spi_interface spi;
	/** Bit-bashing interface */
	struct bit_basher basher;
	/** Currently selected slave
	 *
	 * Valid only when a slave is actually selected.
	 */
	unsigned int slave;
};

/** Bit indices used for SPI bit-bashing interface */
enum {
	/** Serial clock */
	SPI_BIT_SCLK = 0,
	/** Master Out Slave In */
	SPI_BIT_MOSI,
	/** Master In Slave Out */
	SPI_BIT_MISO,
	/** Slave 0 select */
	SPI_BIT_SS0,
};

/**
 * Determine bit index for a particular slave
 *
 * @v slave		Slave number
 * @ret index		Bit index (i.e. SPI_BIT_SSN, where N=slave) 
 */
#define SPI_BIT_SS( slave ) ( SPI_BIT_SS0 + (slave) )

/** Delay between SCLK transitions */
#define SPI_UDELAY 1

extern void init_spi_bit_basher ( struct spi_bit_basher *spibit );

#endif /* _GPXE_SPI_H */
