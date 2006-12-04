#ifndef _GPXE_SPI_H
#define _GPXE_SPI_H

/** @file
 *
 * SPI interface
 *
 */

#include <gpxe/bitbash.h>

/**
 * @defgroup spicmds SPI commands
 * @{
 */

/** Write status register */
#define SPI_WRSR 0x01

/** Write data to memory array */
#define SPI_WRITE 0x02

/** Read data from memory array */
#define SPI_READ 0x03

/** Reset write enable latch */
#define SPI_WRDI 0x04

/** Read status register */
#define SPI_RDSR 0x05

/** Set write enable latch */
#define SPI_WREN 0x06

/**
 * @defgroup atmelcmds Atmel-specific SPI commands
 * @{
 */

/** Erase one sector in memory array (Not supported on all devices) */
#define ATMEL_SECTOR_ERASE 0x52

/** Erase all sections in memory array (Not supported on all devices) */
#define ATMEL_CHIP_ERASE 0x62

/** Read manufacturer and product ID (Not supported on all devices) */
#define ATMEL_RDID 0x15

/** @} */

/** @} */

/**
 * @defgroup spistatus SPI status register bits (not present on all devices)
 * @{
 */

/** Write-protect pin enabled */
#define SPI_STATUS_WPEN 0x80

/** Block protection bit 2 */
#define SPI_STATUS_BP2 0x10

/** Block protection bit 1 */
#define SPI_STATUS_BP1 0x08

/** Block protection bit 0 */
#define SPI_STATUS_BP0 0x04

/** State of the write enable latch */
#define SPI_STATUS_WEN 0x02

/** Device busy flag */
#define SPI_STATUS_NRDY 0x01

/** @} */

struct spi_device;

/**
 * An SPI device type
 *
 * This data structure represents all the characteristics belonging to
 * a particular type of SPI device, e.g. "an Atmel 251024 serial flash",
 * or "a Microchip 25040 serial EEPROM".
 */
struct spi_device_type {
	/** Word length, in bits */
	unsigned int word_len;
	/** Device size (in words) */
	unsigned int size;
	/** Data block size (in words)
	 *
	 * This is the block size used by the device.  It must be a
	 * power of two.  Data reads and writes must not cross a block
	 * boundary.
	 *
	 * Many devices allow reads to cross a block boundary, and
	 * restrict only writes.  For the sake of simplicity, we
	 * assume that the same restriction applies to both reads and
	 * writes.
	 */
	unsigned int block_size;
	/** Command length, in bits */
	unsigned int command_len;
	/** Address length, in bits */
	unsigned int address_len;
	/** Address is munged
	 *
	 * Some devices with 9-bit addresses (e.g. AT25040A EEPROM)
	 * use bit 3 of the command byte as address bit A8, rather
	 * than having a two-byte address.  If this flag is set, then
	 * commands should be munged in this way.
	 */
	unsigned int munge_address : 1;
	/** Read data from device
	 *
	 * @v device		SPI device
	 * @v address		Address from which to read
	 * @v data		Data buffer
	 * @v len		Length of data buffer
	 * @ret rc		Return status code
	 */
	int ( * read ) ( struct spi_device *device, unsigned int address,
			 void *data, size_t len );
	/** Write data to device
	 *
	 * @v device		SPI device
	 * @v address		Address to which to write
	 * @v data		Data buffer
	 * @v len		Length of data buffer
	 * @ret rc		Return status code
	 */
	int ( * write ) ( struct spi_device *device, unsigned int address,
			  const void *data, size_t len );
};

/**
 * @defgroup spidevs SPI device types
 * @{
 */

/** Atmel AT25010 serial EEPROM */
#define AT25010 {		\
	.word_len = 8,		\
	.size = 128,		\
	.block_size = 8,	\
	.command_len = 8,	\
	.address_len = 8,	\
	}

/** @} */

/**
 * An SPI device
 *
 * This data structure represents a real, physical SPI device attached
 * to an SPI controller.  It comprises the device type plus
 * instantiation-specific information such as the slave number.
 */
struct spi_device {
	/** SPI device type */
	struct spi_device_type *type;
	/** SPI bus to which device is attached */
	struct spi_bus *bus;
	/** Slave number */
	unsigned int slave;
};

/**
 * An SPI bus
 *
 * 
 */
struct spi_bus {
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
	 * Read/write data via SPI bus
	 *
	 * @v bus		SPI bus
	 * @v device		SPI device
	 * @v command		Command
	 * @v address		Address to read/write (<0 for no address)
	 * @v data_out		TX data buffer (or NULL)
	 * @v data_in		RX data buffer (or NULL)
	 * @v len		Length of data buffer(s)
	 *
	 * This issues the specified command and optional address to
	 * the SPI device, then reads and/or writes data to/from the
	 * data buffers.
	 */
	int ( * rw ) ( struct spi_bus *bus, struct spi_device *device,
		       unsigned int command, int address,
		       const void *data_out, void *data_in, size_t len );
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

#endif /* _GPXE_SPI_H */
