#ifndef _GPXE_I2C_H
#define _GPXE_I2C_H

/** @file
 *
 * I2C interface
 *
 */

#include <stdint.h>

/** An I2C device
 *
 * An I2C device represents a specific slave device on an I2C bus.  It
 * is accessed via an I2C interface.
 */
struct i2c_device {
	/** Address of this device */
	unsigned int address;
	/** Flag indicating a ten-bit address format */
	int tenbit;
};

/** An I2C interface
 *
 * An I2C interface provides access to an I2C bus, via which I2C
 * devices may be reached.
 */
struct i2c_interface {
	/**
	 * Read data from I2C device
	 *
	 * @v i2c		I2C interface
	 * @v i2cdev		I2C device
	 * @v offset		Starting offset within the device
	 * @v data		Data buffer
	 * @v len		Length of data buffer
	 * @ret rc		Return status code
	 */
	int ( * read ) ( struct i2c_interface *i2c, struct i2c_device *i2cdev,
			 unsigned int offset, uint8_t *data,
			 unsigned int len );
	/**
	 * Write data to I2C device
	 *
	 * @v i2c		I2C interface
	 * @v i2cdev		I2C device
	 * @v offset		Starting offset within the device
	 * @v data		Data buffer
	 * @v len		Length of data buffer
	 * @ret rc		Return status code
	 */
	int ( * write ) ( struct i2c_interface *i2c, struct i2c_device *i2cdev,
			  unsigned int offset, const uint8_t *data,
			  unsigned int len );
};

/** A bit-bashing I2C interface
 *
 * This provides a standardised way to construct I2C buses via a
 * bit-bashing interface.
 */
struct i2c_bit_basher {
	/** I2C interface */
	struct i2c_interface i2c;
	/** Bit-bashing interface */
	struct bit_basher basher;
};

/** Ten-bit address marker
 *
 * This value is ORed with the I2C device address to indicate a
 * ten-bit address format on the bus.
 */
#define I2C_TENBIT_ADDRESS 0x7800

/** An I2C write command */
#define I2C_WRITE 0

/** An I2C read command */
#define I2C_READ 1

/** Bit indices used for I2C bit-bashing interface */
enum {
	/** Serial clock */
	I2C_BIT_SCL = 0,
	/** Serial data */
	I2C_BIT_SDA,
};

/** Delay required for bit-bashing operation */
#define I2C_UDELAY 5

/**
 * Check presence of I2C device
 *
 * @v i2c		I2C interface
 * @v i2cdev		I2C device
 * @ret rc		Return status code
 *
 * Checks for the presence of the device on the I2C bus by attempting
 * a zero-length write.
 */
static inline int i2c_check_presence ( struct i2c_interface *i2c,
				       struct i2c_device *i2cdev ) {
	return i2c->write ( i2c, i2cdev, 0, NULL, 0 );
}

extern void init_i2c_bit_basher ( struct i2c_bit_basher *i2cbit );

#endif /* _GPXE_I2C_H */
