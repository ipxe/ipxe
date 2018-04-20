/*
 * Copyright (C) 2018 Sylvie Barlow <sylvie.c.barlow@gmail.com>.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <unistd.h>
#include <ipxe/bitbash.h>
#include <ipxe/mii_bit.h>

/**
 * Transfer bits over MII bit-bashing interface
 *
 * @v basher		Bit basher
 * @v mask		Mask
 * @v write		Data to write
 * @ret read		Data read
 */
static uint32_t mii_bit_xfer ( struct bit_basher *basher,
			       uint32_t mask, uint32_t write ) {
	uint32_t read = 0;
	int bit;

	for ( ; mask ; mask >>= 1 ) {

		/* Delay */
		udelay ( 1 );

		/* Write bit to basher */
		write_bit ( basher, MII_BIT_MDIO, ( write & mask ) );

		/* Read bit from basher */
		bit = read_bit ( basher, MII_BIT_MDIO );
		read <<= 1;
		read |= ( bit & 1 );

		/* Set clock high */
		write_bit ( basher, MII_BIT_MDC, 1 );

		/* Delay */
		udelay ( 1 );

		/* Set clock low */
		write_bit ( basher, MII_BIT_MDC, 0 );
	}
	return read;
}

/**
 * Read or write via MII bit-bashing interface
 *
 * @v basher		Bit basher
 * @v phy		PHY address
 * @v reg		Register address
 * @v data		Data to write
 * @v cmd		Command
 * @ret data		Data read
 */
static unsigned int mii_bit_rw ( struct bit_basher *basher,
				 unsigned int phy, unsigned int reg,
				 unsigned int data, unsigned int cmd ) {

	/* Initiate drive for write */
	write_bit ( basher, MII_BIT_DRIVE, 1 );

	/* Write start */
	mii_bit_xfer ( basher, MII_BIT_START_MASK, MII_BIT_START );

	/* Write command */
	mii_bit_xfer ( basher, MII_BIT_CMD_MASK, cmd );

	/* Write PHY address */
	mii_bit_xfer ( basher, MII_BIT_PHY_MASK, phy );

	/* Write register address */
	mii_bit_xfer ( basher, MII_BIT_REG_MASK, reg );

	/* Switch drive to read if applicable */
	write_bit ( basher, MII_BIT_DRIVE, ( cmd & MII_BIT_CMD_RW ) );

	/* Allow space for turnaround */
	mii_bit_xfer ( basher, MII_BIT_SWITCH_MASK, MII_BIT_SWITCH );

	/* Read or write data */
	data = mii_bit_xfer (basher, MII_BIT_DATA_MASK, data );

	/* Initiate drive for read */
	write_bit ( basher, MII_BIT_DRIVE, 0 );

	return data;
}

/**
 * Read from MII register
 *
 * @v mdio		MII interface
 * @v phy		PHY address
 * @v reg		Register address
 * @ret data		Data read, or negative error
 */
static int mii_bit_read ( struct mii_interface *mdio, unsigned int phy,
			  unsigned int reg ) {
	struct mii_bit_basher *miibit =
		container_of ( mdio, struct mii_bit_basher, mdio );
	struct bit_basher *basher = &miibit->basher;

	return mii_bit_rw ( basher, phy, reg, 0, MII_BIT_CMD_READ );
}

/**
 * Write to MII register
 *
 * @v mdio		MII interface
 * @v phy		PHY address
 * @v reg		Register address
 * @v data		Data to write
 * @ret rc		Return status code
 */
static int mii_bit_write ( struct mii_interface *mdio, unsigned int phy,
			   unsigned int reg, unsigned int data ) {
	struct mii_bit_basher *miibit =
		container_of ( mdio, struct mii_bit_basher, mdio );
	struct bit_basher *basher = &miibit->basher;

	mii_bit_rw ( basher, phy, reg, data, MII_BIT_CMD_WRITE );
	return 0;
}

/** MII bit basher operations */
static struct mii_operations mii_bit_op = {
	.read = mii_bit_read,
	.write = mii_bit_write,
};

/**
 * Initialise bit-bashing interface
 *
 * @v miibit		MII bit basher
 */
void init_mii_bit_basher ( struct mii_bit_basher *miibit ) {
	mdio_init ( &miibit->mdio, &mii_bit_op );
};
