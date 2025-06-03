/*
 * Copyright (C) 2017 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ipxe/usb.h>
#include <ipxe/usbnet.h>
#include <ipxe/ethernet.h>
#include <ipxe/profile.h>
#include <ipxe/fdt.h>
#include "smscusb.h"

/** @file
 *
 * SMSC USB Ethernet drivers
 *
 */

/** Interrupt completion profiler */
static struct profiler smscusb_intr_profiler __profiler =
	{ .name = "smscusb.intr" };

/******************************************************************************
 *
 * Register access
 *
 ******************************************************************************
 */

/**
 * Write register (without byte-swapping)
 *
 * @v smscusb		Smscusb device
 * @v address		Register address
 * @v value		Register value
 * @ret rc		Return status code
 */
int smscusb_raw_writel ( struct smscusb_device *smscusb, unsigned int address,
			 uint32_t value ) {
	int rc;

	/* Write register */
	DBGCIO ( smscusb, "SMSCUSB %p [%03x] <= %08x\n",
		 smscusb, address, le32_to_cpu ( value ) );
	if ( ( rc = usb_control ( smscusb->usb, SMSCUSB_REGISTER_WRITE, 0,
				  address, &value, sizeof ( value ) ) ) != 0 ) {
		DBGC ( smscusb, "SMSCUSB %p could not write %03x: %s\n",
		       smscusb, address, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Read register (without byte-swapping)
 *
 * @v smscusb		SMSC USB device
 * @v address		Register address
 * @ret value		Register value
 * @ret rc		Return status code
 */
int smscusb_raw_readl ( struct smscusb_device *smscusb, unsigned int address,
			uint32_t *value ) {
	int rc;

	/* Read register */
	if ( ( rc = usb_control ( smscusb->usb, SMSCUSB_REGISTER_READ, 0,
				  address, value, sizeof ( *value ) ) ) != 0 ) {
		DBGC ( smscusb, "SMSCUSB %p could not read %03x: %s\n",
		       smscusb, address, strerror ( rc ) );
		return rc;
	}
	DBGCIO ( smscusb, "SMSCUSB %p [%03x] => %08x\n",
		 smscusb, address, le32_to_cpu ( *value ) );

	return 0;
}

/******************************************************************************
 *
 * EEPROM access
 *
 ******************************************************************************
 */

/**
 * Wait for EEPROM to become idle
 *
 * @v smscusb		SMSC USB device
 * @v e2p_base		E2P register base
 * @ret rc		Return status code
 */
static int smscusb_eeprom_wait ( struct smscusb_device *smscusb,
				 unsigned int e2p_base ) {
	uint32_t e2p_cmd;
	unsigned int i;
	int rc;

	/* Wait for EPC_BSY to become clear */
	for ( i = 0 ; i < SMSCUSB_EEPROM_MAX_WAIT_MS ; i++ ) {

		/* Read E2P_CMD and check EPC_BSY */
		if ( ( rc = smscusb_readl ( smscusb,
					    ( e2p_base + SMSCUSB_E2P_CMD ),
					    &e2p_cmd ) ) != 0 )
			return rc;
		if ( ! ( e2p_cmd & SMSCUSB_E2P_CMD_EPC_BSY ) )
			return 0;

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( smscusb, "SMSCUSB %p timed out waiting for EEPROM\n",
	       smscusb );
	return -ETIMEDOUT;
}

/**
 * Read byte from EEPROM
 *
 * @v smscusb		SMSC USB device
 * @v e2p_base		E2P register base
 * @v address		EEPROM address
 * @ret byte		Byte read, or negative error
 */
static int smscusb_eeprom_read_byte ( struct smscusb_device *smscusb,
				      unsigned int e2p_base,
				      unsigned int address ) {
	uint32_t e2p_cmd;
	uint32_t e2p_data;
	int rc;

	/* Wait for EEPROM to become idle */
	if ( ( rc = smscusb_eeprom_wait ( smscusb, e2p_base ) ) != 0 )
		return rc;

	/* Initiate read command */
	e2p_cmd = ( SMSCUSB_E2P_CMD_EPC_BSY | SMSCUSB_E2P_CMD_EPC_CMD_READ |
		    SMSCUSB_E2P_CMD_EPC_ADDR ( address ) );
	if ( ( rc = smscusb_writel ( smscusb, ( e2p_base + SMSCUSB_E2P_CMD ),
				     e2p_cmd ) ) != 0 )
		return rc;

	/* Wait for command to complete */
	if ( ( rc = smscusb_eeprom_wait ( smscusb, e2p_base ) ) != 0 )
		return rc;

	/* Read EEPROM data */
	if ( ( rc = smscusb_readl ( smscusb, ( e2p_base + SMSCUSB_E2P_DATA ),
				    &e2p_data ) ) != 0 )
		return rc;

	return SMSCUSB_E2P_DATA_GET ( e2p_data );
}

/**
 * Read data from EEPROM
 *
 * @v smscusb		SMSC USB device
 * @v e2p_base		E2P register base
 * @v address		EEPROM address
 * @v data		Data buffer
 * @v len		Length of data
 * @ret rc		Return status code
 */
static int smscusb_eeprom_read ( struct smscusb_device *smscusb,
				 unsigned int e2p_base, unsigned int address,
				 void *data, size_t len ) {
	uint8_t *bytes;
	int byte;

	/* Read bytes */
	for ( bytes = data ; len-- ; address++, bytes++ ) {
		byte = smscusb_eeprom_read_byte ( smscusb, e2p_base, address );
		if ( byte < 0 )
			return byte;
		*bytes = byte;
	}

	return 0;
}

/**
 * Fetch MAC address from EEPROM
 *
 * @v smscusb		SMSC USB device
 * @v e2p_base		E2P register base
 * @ret rc		Return status code
 */
int smscusb_eeprom_fetch_mac ( struct smscusb_device *smscusb,
			       unsigned int e2p_base ) {
	struct net_device *netdev = smscusb->netdev;
	int rc;

	/* Read MAC address from EEPROM */
	if ( ( rc = smscusb_eeprom_read ( smscusb, e2p_base, SMSCUSB_EEPROM_MAC,
					  netdev->hw_addr, ETH_ALEN ) ) != 0 )
		return rc;

	/* Check that EEPROM is physically present */
	if ( ! is_valid_ether_addr ( netdev->hw_addr ) ) {
		DBGC ( smscusb, "SMSCUSB %p has no EEPROM MAC (%s)\n",
		       smscusb, eth_ntoa ( netdev->hw_addr ) );
		return -ENODEV;
	}

	DBGC ( smscusb, "SMSCUSB %p using EEPROM MAC %s\n",
	       smscusb, eth_ntoa ( netdev->hw_addr ) );
	return 0;
}

/******************************************************************************
 *
 * OTP access
 *
 ******************************************************************************
 */

/**
 * Power up OTP
 *
 * @v smscusb		SMSC USB device
 * @v otp_base		OTP register base
 * @ret rc		Return status code
 */
static int smscusb_otp_power_up ( struct smscusb_device *smscusb,
				  unsigned int otp_base ) {
	uint32_t otp_power;
	unsigned int i;
	int rc;

	/* Power up OTP */
	if ( ( rc = smscusb_writel ( smscusb, ( otp_base + SMSCUSB_OTP_POWER ),
				     0 ) ) != 0 )
		return rc;

	/* Wait for OTP_POWER_DOWN to become clear */
	for ( i = 0 ; i < SMSCUSB_OTP_MAX_WAIT_MS ; i++ ) {

		/* Read OTP_POWER and check OTP_POWER_DOWN */
		if ( ( rc = smscusb_readl ( smscusb,
					    ( otp_base + SMSCUSB_OTP_POWER ),
					    &otp_power ) ) != 0 )
			return rc;
		if ( ! ( otp_power & SMSCUSB_OTP_POWER_DOWN ) )
			return 0;

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( smscusb, "SMSCUSB %p timed out waiting for OTP power up\n",
	       smscusb );
	return -ETIMEDOUT;
}

/**
 * Wait for OTP to become idle
 *
 * @v smscusb		SMSC USB device
 * @v otp_base		OTP register base
 * @ret rc		Return status code
 */
static int smscusb_otp_wait ( struct smscusb_device *smscusb,
			      unsigned int otp_base ) {
	uint32_t otp_status;
	unsigned int i;
	int rc;

	/* Wait for OTP_STATUS_BUSY to become clear */
	for ( i = 0 ; i < SMSCUSB_OTP_MAX_WAIT_MS ; i++ ) {

		/* Read OTP_STATUS and check OTP_STATUS_BUSY */
		if ( ( rc = smscusb_readl ( smscusb,
					    ( otp_base + SMSCUSB_OTP_STATUS ),
					    &otp_status ) ) != 0 )
			return rc;
		if ( ! ( otp_status & SMSCUSB_OTP_STATUS_BUSY ) )
			return 0;

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( smscusb, "SMSCUSB %p timed out waiting for OTP\n",
	       smscusb );
	return -ETIMEDOUT;
}

/**
 * Read byte from OTP
 *
 * @v smscusb		SMSC USB device
 * @v otp_base		OTP register base
 * @v address		OTP address
 * @ret byte		Byte read, or negative error
 */
static int smscusb_otp_read_byte ( struct smscusb_device *smscusb,
				   unsigned int otp_base,
				   unsigned int address ) {
	uint8_t addrh = ( address >> 8 );
	uint8_t addrl = ( address >> 0 );
	uint32_t otp_data;
	int rc;

	/* Wait for OTP to become idle */
	if ( ( rc = smscusb_otp_wait ( smscusb, otp_base ) ) != 0 )
		return rc;

	/* Initiate read command */
	if ( ( rc = smscusb_writel ( smscusb, ( otp_base + SMSCUSB_OTP_ADDRH ),
				     addrh ) ) != 0 )
		return rc;
	if ( ( rc = smscusb_writel ( smscusb, ( otp_base + SMSCUSB_OTP_ADDRL ),
				     addrl ) ) != 0 )
		return rc;
	if ( ( rc = smscusb_writel ( smscusb, ( otp_base + SMSCUSB_OTP_CMD ),
				     SMSCUSB_OTP_CMD_READ ) ) != 0 )
		return rc;
	if ( ( rc = smscusb_writel ( smscusb, ( otp_base + SMSCUSB_OTP_GO ),
				     SMSCUSB_OTP_GO_GO ) ) != 0 )
		return rc;

	/* Wait for command to complete */
	if ( ( rc = smscusb_otp_wait ( smscusb, otp_base ) ) != 0 )
		return rc;

	/* Read OTP data */
	if ( ( rc = smscusb_readl ( smscusb, ( otp_base + SMSCUSB_OTP_DATA ),
				    &otp_data ) ) != 0 )
		return rc;

	return SMSCUSB_OTP_DATA_GET ( otp_data );
}

/**
 * Read data from OTP
 *
 * @v smscusb		SMSC USB device
 * @v otp_base		OTP register base
 * @v address		OTP address
 * @v data		Data buffer
 * @v len		Length of data
 * @ret rc		Return status code
 */
static int smscusb_otp_read ( struct smscusb_device *smscusb,
			      unsigned int otp_base, unsigned int address,
			      void *data, size_t len ) {
	uint8_t *bytes;
	int byte;
	int rc;

	/* Power up OTP */
	if ( ( rc = smscusb_otp_power_up ( smscusb, otp_base ) ) != 0 )
		return rc;

	/* Read bytes */
	for ( bytes = data ; len-- ; address++, bytes++ ) {
		byte = smscusb_otp_read_byte ( smscusb, otp_base, address );
		if ( byte < 0 )
			return byte;
		*bytes = byte;
	}

	return 0;
}

/**
 * Fetch MAC address from OTP
 *
 * @v smscusb		SMSC USB device
 * @v otp_base		OTP register base
 * @ret rc		Return status code
 */
int smscusb_otp_fetch_mac ( struct smscusb_device *smscusb,
			    unsigned int otp_base ) {
	struct net_device *netdev = smscusb->netdev;
	uint8_t signature;
	unsigned int address;
	int rc;

	/* Read OTP signature byte */
	if ( ( rc = smscusb_otp_read ( smscusb, otp_base, 0, &signature,
				       sizeof ( signature ) ) ) != 0 )
		return rc;

	/* Determine location of MAC address */
	switch ( signature ) {
	case SMSCUSB_OTP_1_SIG:
		address = SMSCUSB_OTP_1_MAC;
		break;
	case SMSCUSB_OTP_2_SIG:
		address = SMSCUSB_OTP_2_MAC;
		break;
	default:
		DBGC ( smscusb, "SMSCUSB %p unknown OTP signature %#02x\n",
		       smscusb, signature );
		return -ENOTSUP;
	}

	/* Read MAC address from OTP */
	if ( ( rc = smscusb_otp_read ( smscusb, otp_base, address,
				       netdev->hw_addr, ETH_ALEN ) ) != 0 )
		return rc;

	/* Check that OTP is valid */
	if ( ! is_valid_ether_addr ( netdev->hw_addr ) ) {
		DBGC ( smscusb, "SMSCUSB %p has no layout %#02x OTP MAC (%s)\n",
		       smscusb, signature, eth_ntoa ( netdev->hw_addr ) );
		return -ENODEV;
	}

	DBGC ( smscusb, "SMSCUSB %p using layout %#02x OTP MAC %s\n",
	       smscusb, signature, eth_ntoa ( netdev->hw_addr ) );
	return 0;
}

/******************************************************************************
 *
 * Device tree
 *
 ******************************************************************************
 */

/**
 * Fetch MAC address from device tree
 *
 * @v smscusb		SMSC USB device
 * @ret rc		Return status code
 */
int smscusb_fdt_fetch_mac ( struct smscusb_device *smscusb ) {
	struct net_device *netdev = smscusb->netdev;
	unsigned int offset;
	int rc;

	/* Look for "ethernet[0]" alias */
	if ( ( rc = fdt_alias ( &sysfdt, "ethernet", &offset ) != 0 ) &&
	     ( rc = fdt_alias ( &sysfdt, "ethernet0", &offset ) != 0 ) ) {
		return rc;
	}

	/* Fetch MAC address */
	if ( ( rc = fdt_mac ( &sysfdt, offset, netdev ) ) != 0 )
		return rc;

	DBGC ( smscusb, "SMSCUSB %p using FDT MAC %s\n",
	       smscusb, eth_ntoa ( netdev->hw_addr ) );
	return 0;
}

/******************************************************************************
 *
 * MII access
 *
 ******************************************************************************
 */

/**
 * Wait for MII to become idle
 *
 * @v smscusb		SMSC USB device
 * @ret rc		Return status code
 */
static int smscusb_mii_wait ( struct smscusb_device *smscusb ) {
	unsigned int base = smscusb->mii_base;
	uint32_t mii_access;
	unsigned int i;
	int rc;

	/* Wait for MIIBZY to become clear */
	for ( i = 0 ; i < SMSCUSB_MII_MAX_WAIT_MS ; i++ ) {

		/* Read MII_ACCESS and check MIIBZY */
		if ( ( rc = smscusb_readl ( smscusb,
					    ( base + SMSCUSB_MII_ACCESS ),
					    &mii_access ) ) != 0 )
			return rc;
		if ( ! ( mii_access & SMSCUSB_MII_ACCESS_MIIBZY ) )
			return 0;

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( smscusb, "SMSCUSB %p timed out waiting for MII\n",
	       smscusb );
	return -ETIMEDOUT;
}

/**
 * Read from MII register
 *
 * @v mdio		MII interface
 * @v phy		PHY address
 * @v reg		Register address
 * @ret value		Data read, or negative error
 */
static int smscusb_mii_read ( struct mii_interface *mdio,
			      unsigned int phy __unused, unsigned int reg ) {
	struct smscusb_device *smscusb =
		container_of ( mdio, struct smscusb_device, mdio );
	unsigned int base = smscusb->mii_base;
	uint32_t mii_access;
	uint32_t mii_data;
	int rc;

	/* Wait for MII to become idle */
	if ( ( rc = smscusb_mii_wait ( smscusb ) ) != 0 )
		return rc;

	/* Initiate read command */
	mii_access = ( SMSCUSB_MII_ACCESS_PHY_ADDRESS |
		       SMSCUSB_MII_ACCESS_MIIRINDA ( reg ) |
		       SMSCUSB_MII_ACCESS_MIIBZY );
	if ( ( rc = smscusb_writel ( smscusb, ( base + SMSCUSB_MII_ACCESS ),
				     mii_access ) ) != 0 )
		return rc;

	/* Wait for command to complete */
	if ( ( rc = smscusb_mii_wait ( smscusb ) ) != 0 )
		return rc;

	/* Read MII data */
	if ( ( rc = smscusb_readl ( smscusb, ( base + SMSCUSB_MII_DATA ),
				    &mii_data ) ) != 0 )
		return rc;

	return SMSCUSB_MII_DATA_GET ( mii_data );
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
static int smscusb_mii_write ( struct mii_interface *mdio,
			       unsigned int phy __unused, unsigned int reg,
			       unsigned int data ) {
	struct smscusb_device *smscusb =
		container_of ( mdio, struct smscusb_device, mdio );
	unsigned int base = smscusb->mii_base;
	uint32_t mii_access;
	uint32_t mii_data;
	int rc;

	/* Wait for MII to become idle */
	if ( ( rc = smscusb_mii_wait ( smscusb ) ) != 0 )
		return rc;

	/* Write MII data */
	mii_data = SMSCUSB_MII_DATA_SET ( data );
	if ( ( rc = smscusb_writel ( smscusb, ( base + SMSCUSB_MII_DATA ),
				     mii_data ) ) != 0 )
		return rc;

	/* Initiate write command */
	mii_access = ( SMSCUSB_MII_ACCESS_PHY_ADDRESS |
		       SMSCUSB_MII_ACCESS_MIIRINDA ( reg ) |
		       SMSCUSB_MII_ACCESS_MIIWNR |
		       SMSCUSB_MII_ACCESS_MIIBZY );
	if ( ( rc = smscusb_writel ( smscusb, ( base + SMSCUSB_MII_ACCESS ),
				     mii_access ) ) != 0 )
		return rc;

	/* Wait for command to complete */
	if ( ( rc = smscusb_mii_wait ( smscusb ) ) != 0 )
		return rc;

	return 0;
}

/** MII operations */
struct mii_operations smscusb_mii_operations = {
	.read = smscusb_mii_read,
	.write = smscusb_mii_write,
};

/**
 * Check link status
 *
 * @v smscusb		SMSC USB device
 * @ret rc		Return status code
 */
int smscusb_mii_check_link ( struct smscusb_device *smscusb ) {
	struct net_device *netdev = smscusb->netdev;
	int intr;
	int rc;

	/* Read PHY interrupt source */
	intr = mii_read ( &smscusb->mii, smscusb->phy_source );
	if ( intr < 0 ) {
		rc = intr;
		DBGC ( smscusb, "SMSCUSB %p could not get PHY interrupt "
		       "source: %s\n", smscusb, strerror ( rc ) );
		return rc;
	}

	/* Acknowledge PHY interrupt */
	if ( ( rc = mii_write ( &smscusb->mii, smscusb->phy_source,
				intr ) ) != 0 ) {
		DBGC ( smscusb, "SMSCUSB %p could not acknowledge PHY "
		       "interrupt: %s\n", smscusb, strerror ( rc ) );
		return rc;
	}

	/* Check link status */
	if ( ( rc = mii_check_link ( &smscusb->mii, netdev ) ) != 0 ) {
		DBGC ( smscusb, "SMSCUSB %p could not check link: %s\n",
		       smscusb, strerror ( rc ) );
		return rc;
	}

	DBGC ( smscusb, "SMSCUSB %p link %s (intr %#04x)\n",
	       smscusb, ( netdev_link_ok ( netdev ) ? "up" : "down" ), intr );
	return 0;
}

/**
 * Enable PHY interrupts and update link status
 *
 * @v smscusb		SMSC USB device
 * @v phy_mask		PHY interrupt mask register
 * @v intrs		PHY interrupts to enable
 * @ret rc		Return status code
 */
int smscusb_mii_open ( struct smscusb_device *smscusb,
		       unsigned int phy_mask, unsigned int intrs ) {
	int rc;

	/* Enable PHY interrupts */
	if ( ( rc = mii_write ( &smscusb->mii, phy_mask, intrs ) ) != 0 ) {
		DBGC ( smscusb, "SMSCUSB %p could not set PHY interrupt "
		       "mask: %s\n", smscusb, strerror ( rc ) );
		return rc;
	}

	/* Update link status */
	smscusb_mii_check_link ( smscusb );

	return 0;
}

/******************************************************************************
 *
 * Receive filtering
 *
 ******************************************************************************
 */

/**
 * Set receive address
 *
 * @v smscusb		SMSC USB device
 * @v addr_base		Receive address register base
 * @ret rc		Return status code
 */
int smscusb_set_address ( struct smscusb_device *smscusb,
			  unsigned int addr_base ) {
	struct net_device *netdev = smscusb->netdev;
	union smscusb_mac mac;
	int rc;

	/* Copy MAC address */
	memset ( &mac, 0, sizeof ( mac ) );
	memcpy ( mac.raw, netdev->ll_addr, ETH_ALEN );

	/* Write MAC address high register */
	if ( ( rc = smscusb_raw_writel ( smscusb,
					 ( addr_base + SMSCUSB_RX_ADDRH ),
					 mac.addr.h ) ) != 0 )
		return rc;

	/* Write MAC address low register */
	if ( ( rc = smscusb_raw_writel ( smscusb,
					 ( addr_base + SMSCUSB_RX_ADDRL ),
					 mac.addr.l ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Set receive filter
 *
 * @v smscusb		SMSC USB device
 * @v filt_base		Receive filter register base
 * @ret rc		Return status code
 */
int smscusb_set_filter ( struct smscusb_device *smscusb,
			 unsigned int filt_base ) {
	struct net_device *netdev = smscusb->netdev;
	union smscusb_mac mac;
	int rc;

	/* Copy MAC address */
	memset ( &mac, 0, sizeof ( mac ) );
	memcpy ( mac.raw, netdev->ll_addr, ETH_ALEN );
	mac.addr.h |= cpu_to_le32 ( SMSCUSB_ADDR_FILTH_VALID );

	/* Write MAC address perfect filter high register */
	if ( ( rc = smscusb_raw_writel ( smscusb,
					 ( filt_base + SMSCUSB_ADDR_FILTH(0) ),
					 mac.addr.h ) ) != 0 )
		return rc;

	/* Write MAC address perfect filter low register */
	if ( ( rc = smscusb_raw_writel ( smscusb,
					 ( filt_base + SMSCUSB_ADDR_FILTL(0) ),
					 mac.addr.l ) ) != 0 )
		return rc;

	return 0;
}

/******************************************************************************
 *
 * Endpoint operations
 *
 ******************************************************************************
 */

/**
 * Complete interrupt transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void smscusb_intr_complete ( struct usb_endpoint *ep,
				    struct io_buffer *iobuf, int rc ) {
	struct smscusb_device *smscusb =
		container_of ( ep, struct smscusb_device, usbnet.intr );
	struct net_device *netdev = smscusb->netdev;
	struct smscusb_interrupt *intr;

	/* Profile completions */
	profile_start ( &smscusb_intr_profiler );

	/* Ignore packets cancelled when the endpoint closes */
	if ( ! ep->open )
		goto done;

	/* Record USB errors against the network device */
	if ( rc != 0 ) {
		DBGC ( smscusb, "SMSCUSB %p interrupt failed: %s\n",
		       smscusb, strerror ( rc ) );
		DBGC_HDA ( smscusb, 0, iobuf->data, iob_len ( iobuf ) );
		netdev_rx_err ( netdev, NULL, rc );
		goto done;
	}

	/* Extract interrupt data */
	if ( iob_len ( iobuf ) != sizeof ( *intr ) ) {
		DBGC ( smscusb, "SMSCUSB %p malformed interrupt\n",
		       smscusb );
		DBGC_HDA ( smscusb, 0, iobuf->data, iob_len ( iobuf ) );
		netdev_rx_err ( netdev, NULL, rc );
		goto done;
	}
	intr = iobuf->data;

	/* Record interrupt status */
	smscusb->int_sts = le32_to_cpu ( intr->int_sts );
	profile_stop ( &smscusb_intr_profiler );

 done:
	/* Free I/O buffer */
	free_iob ( iobuf );
}

/** Interrupt endpoint operations */
struct usb_endpoint_driver_operations smscusb_intr_operations = {
	.complete = smscusb_intr_complete,
};

/**
 * Complete bulk OUT transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void smscusb_out_complete ( struct usb_endpoint *ep,
				   struct io_buffer *iobuf, int rc ) {
	struct smscusb_device *smscusb =
		container_of ( ep, struct smscusb_device, usbnet.out );
	struct net_device *netdev = smscusb->netdev;

	/* Report TX completion */
	netdev_tx_complete_err ( netdev, iobuf, rc );
}

/** Bulk OUT endpoint operations */
struct usb_endpoint_driver_operations smscusb_out_operations = {
	.complete = smscusb_out_complete,
};
