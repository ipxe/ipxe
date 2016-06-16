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
#include <unistd.h>
#include <errno.h>
#include <ipxe/ethernet.h>
#include <ipxe/usb.h>
#include <ipxe/usbnet.h>
#include "lan78xx.h"

/** @file
 *
 * Microchip LAN78xx USB Ethernet driver
 *
 */

/******************************************************************************
 *
 * MAC address
 *
 ******************************************************************************
 */

/**
 * Fetch MAC address from EEPROM
 *
 * @v smscusb		SMSC USB device
 * @ret rc		Return status code
 */
static int lan78xx_eeprom_fetch_mac ( struct smscusb_device *smscusb ) {
	uint32_t hw_cfg;
	uint32_t orig_hw_cfg;
	int rc;

	/* Read original HW_CFG value */
	if ( ( rc = smscusb_readl ( smscusb, LAN78XX_HW_CFG, &hw_cfg ) ) != 0 )
		goto err_read_hw_cfg;
	orig_hw_cfg = hw_cfg;

	/* Temporarily disable LED0 and LED1 (which share physical
	 * pins with EEDO and EECLK respectively).
	 */
	hw_cfg &= ~( LAN78XX_HW_CFG_LED0_EN | LAN78XX_HW_CFG_LED1_EN );
	if ( ( rc = smscusb_writel ( smscusb, LAN78XX_HW_CFG, hw_cfg ) ) != 0 )
		goto err_write_hw_cfg;

	/* Fetch MAC address from EEPROM */
	if ( ( rc = smscusb_eeprom_fetch_mac ( smscusb,
					       LAN78XX_E2P_BASE ) ) != 0 )
		goto err_fetch_mac;

 err_fetch_mac:
	smscusb_writel ( smscusb, LAN78XX_HW_CFG, orig_hw_cfg );
 err_write_hw_cfg:
 err_read_hw_cfg:
	return rc;
}

/**
 * Fetch MAC address
 *
 * @v smscusb		SMSC USB device
 * @ret rc		Return status code
 */
static int lan78xx_fetch_mac ( struct smscusb_device *smscusb ) {
	struct net_device *netdev = smscusb->netdev;
	int rc;

	/* Read MAC address from EEPROM, if present */
	if ( ( rc = lan78xx_eeprom_fetch_mac ( smscusb ) ) == 0 )
		return 0;

	/* Read MAC address from OTP, if present */
	if ( ( rc = smscusb_otp_fetch_mac ( smscusb, LAN78XX_OTP_BASE ) ) == 0 )
		return 0;

	/* Otherwise, generate a random MAC address */
	eth_random_addr ( netdev->hw_addr );
	DBGC ( smscusb, "LAN78XX %p using random MAC %s\n",
	       smscusb, eth_ntoa ( netdev->hw_addr ) );
	return 0;
}

/******************************************************************************
 *
 * Device reset
 *
 ******************************************************************************
 */

/**
 * Reset device
 *
 * @v smscusb		SMSC USB device
 * @ret rc		Return status code
 */
static int lan78xx_reset ( struct smscusb_device *smscusb ) {
	uint32_t hw_cfg;
	unsigned int i;
	int rc;

	/* Reset device */
	if ( ( rc = smscusb_writel ( smscusb, LAN78XX_HW_CFG,
				     LAN78XX_HW_CFG_LRST ) ) != 0 )
		return rc;

	/* Wait for reset to complete */
	for ( i = 0 ; i < LAN78XX_RESET_MAX_WAIT_MS ; i++ ) {

		/* Check if reset has completed */
		if ( ( rc = smscusb_readl ( smscusb, LAN78XX_HW_CFG,
					    &hw_cfg ) ) != 0 )
			return rc;
		if ( ! ( hw_cfg & LAN78XX_HW_CFG_LRST ) )
			return 0;

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( smscusb, "LAN78XX %p timed out waiting for reset\n",
	       smscusb );
	return -ETIMEDOUT;
}

/******************************************************************************
 *
 * Network device interface
 *
 ******************************************************************************
 */

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int lan78xx_open ( struct net_device *netdev ) {
	struct smscusb_device *smscusb = netdev->priv;
	uint32_t usb_cfg0;
	int rc;

	/* Clear stored interrupt status */
	smscusb->int_sts = 0;

	/* Configure bulk IN empty response */
	if ( ( rc = smscusb_readl ( smscusb, LAN78XX_USB_CFG0,
				    &usb_cfg0 ) ) != 0 )
		goto err_usb_cfg0_read;
	usb_cfg0 |= LAN78XX_USB_CFG0_BIR;
	if ( ( rc = smscusb_writel ( smscusb, LAN78XX_USB_CFG0,
				     usb_cfg0 ) ) != 0 )
		goto err_usb_cfg0_write;

	/* Open USB network device */
	if ( ( rc = usbnet_open ( &smscusb->usbnet ) ) != 0 ) {
		DBGC ( smscusb, "LAN78XX %p could not open: %s\n",
		       smscusb, strerror ( rc ) );
		goto err_open;
	}

	/* Configure interrupt endpoint */
	if ( ( rc = smscusb_writel ( smscusb, LAN78XX_INT_EP_CTL,
				     ( LAN78XX_INT_EP_CTL_RDFO_EN |
				       LAN78XX_INT_EP_CTL_PHY_EN ) ) ) != 0 )
		goto err_int_ep_ctl;

	/* Configure bulk IN delay */
	if ( ( rc = smscusb_writel ( smscusb, LAN78XX_BULK_IN_DLY,
				     LAN78XX_BULK_IN_DLY_SET ( 0 ) ) ) != 0 )
		goto err_bulk_in_dly;

	/* Configure receive filters */
	if ( ( rc = smscusb_writel ( smscusb, LAN78XX_RFE_CTL,
				     ( LAN78XX_RFE_CTL_AB |
				       LAN78XX_RFE_CTL_AM |
				       LAN78XX_RFE_CTL_AU ) ) ) != 0 )
		goto err_rfe_ctl;

	/* Configure receive FIFO */
	if ( ( rc = smscusb_writel ( smscusb, LAN78XX_FCT_RX_CTL,
				     ( LAN78XX_FCT_RX_CTL_EN |
				       LAN78XX_FCT_RX_CTL_BAD ) ) ) != 0 )
		goto err_fct_rx_ctl;

	/* Configure transmit FIFO */
	if ( ( rc = smscusb_writel ( smscusb, LAN78XX_FCT_TX_CTL,
				     LAN78XX_FCT_TX_CTL_EN ) ) != 0 )
		goto err_fct_tx_ctl;

	/* Configure receive datapath */
	if ( ( rc = smscusb_writel ( smscusb, LAN78XX_MAC_RX,
				     ( LAN78XX_MAC_RX_MAX_SIZE_DEFAULT |
				       LAN78XX_MAC_RX_FCS |
				       LAN78XX_MAC_RX_EN ) ) ) != 0 )
		goto err_mac_rx;

	/* Configure transmit datapath */
	if ( ( rc = smscusb_writel ( smscusb, LAN78XX_MAC_TX,
				     LAN78XX_MAC_TX_EN ) ) != 0 )
		goto err_mac_tx;

	/* Set MAC address */
	if ( ( rc = smscusb_set_address ( smscusb,
					  LAN78XX_RX_ADDR_BASE ) ) != 0 )
		goto err_set_address;

	/* Set MAC address perfect filter */
	if ( ( rc = smscusb_set_filter ( smscusb,
					 LAN78XX_ADDR_FILT_BASE ) ) != 0 )
		goto err_set_filter;

	/* Enable PHY interrupts and update link status */
	if ( ( rc = smscusb_mii_open ( smscusb, LAN78XX_MII_PHY_INTR_MASK,
				       ( LAN78XX_PHY_INTR_ENABLE |
					 LAN78XX_PHY_INTR_LINK |
					 LAN78XX_PHY_INTR_ANEG_ERR |
					 LAN78XX_PHY_INTR_ANEG_DONE ) ) ) != 0 )
		goto err_mii_open;

	return 0;

 err_mii_open:
 err_set_filter:
 err_set_address:
 err_mac_tx:
 err_mac_rx:
 err_fct_tx_ctl:
 err_fct_rx_ctl:
 err_rfe_ctl:
 err_bulk_in_dly:
 err_int_ep_ctl:
	usbnet_close ( &smscusb->usbnet );
 err_open:
 err_usb_cfg0_write:
 err_usb_cfg0_read:
	lan78xx_reset ( smscusb );
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void lan78xx_close ( struct net_device *netdev ) {
	struct smscusb_device *smscusb = netdev->priv;

	/* Close USB network device */
	usbnet_close ( &smscusb->usbnet );

	/* Dump statistics (for debugging) */
	if ( DBG_LOG )
		smsc75xx_dump_statistics ( smscusb );

	/* Reset device */
	lan78xx_reset ( smscusb );
}

/** LAN78xx network device operations */
static struct net_device_operations lan78xx_operations = {
	.open		= lan78xx_open,
	.close		= lan78xx_close,
	.transmit	= smsc75xx_transmit,
	.poll		= smsc75xx_poll,
};

/******************************************************************************
 *
 * USB interface
 *
 ******************************************************************************
 */

/**
 * Probe device
 *
 * @v func		USB function
 * @v config		Configuration descriptor
 * @ret rc		Return status code
 */
static int lan78xx_probe ( struct usb_function *func,
			   struct usb_configuration_descriptor *config ) {
	struct net_device *netdev;
	struct smscusb_device *smscusb;
	int rc;

	/* Allocate and initialise structure */
	netdev = alloc_etherdev ( sizeof ( *smscusb ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &lan78xx_operations );
	netdev->dev = &func->dev;
	smscusb = netdev->priv;
	memset ( smscusb, 0, sizeof ( *smscusb ) );
	smscusb_init ( smscusb, netdev, func, &smsc75xx_in_operations );
	smscusb_mii_init ( smscusb, LAN78XX_MII_BASE,
			   LAN78XX_MII_PHY_INTR_SOURCE );
	usb_refill_init ( &smscusb->usbnet.in, 0, SMSC75XX_IN_MTU,
			  SMSC75XX_IN_MAX_FILL );
	DBGC ( smscusb, "LAN78XX %p on %s\n", smscusb, func->name );

	/* Describe USB network device */
	if ( ( rc = usbnet_describe ( &smscusb->usbnet, config ) ) != 0 ) {
		DBGC ( smscusb, "LAN78XX %p could not describe: %s\n",
		       smscusb, strerror ( rc ) );
		goto err_describe;
	}

	/* Reset device */
	if ( ( rc = lan78xx_reset ( smscusb ) ) != 0 )
		goto err_reset;

	/* Read MAC address */
	if ( ( rc = lan78xx_fetch_mac ( smscusb ) ) != 0 )
		goto err_fetch_mac;

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register;

	usb_func_set_drvdata ( func, netdev );
	return 0;

	unregister_netdev ( netdev );
 err_register:
 err_fetch_mac:
 err_reset:
 err_describe:
	netdev_nullify ( netdev );
	netdev_put ( netdev );
 err_alloc:
	return rc;
}

/**
 * Remove device
 *
 * @v func		USB function
 */
static void lan78xx_remove ( struct usb_function *func ) {
	struct net_device *netdev = usb_func_get_drvdata ( func );

	unregister_netdev ( netdev );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** LAN78xx device IDs */
static struct usb_device_id lan78xx_ids[] = {
	{
		.name = "lan7800",
		.vendor = 0x0424,
		.product = 0x7800,
	},
	{
		.name = "lan7850",
		.vendor = 0x0424,
		.product = 0x7850,
	},
};

/** LAN78xx driver */
struct usb_driver lan78xx_driver __usb_driver = {
	.ids = lan78xx_ids,
	.id_count = ( sizeof ( lan78xx_ids ) / sizeof ( lan78xx_ids[0] ) ),
	.class = USB_CLASS_ID ( 0xff, 0x00, 0xff ),
	.score = USB_SCORE_NORMAL,
	.probe = lan78xx_probe,
	.remove = lan78xx_remove,
};
