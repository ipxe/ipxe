/*
 * Copyright (C) 2015 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <byteswap.h>
#include <ipxe/ethernet.h>
#include <ipxe/usb.h>
#include <ipxe/usbnet.h>
#include <ipxe/profile.h>
#include <ipxe/base16.h>
#include <ipxe/smbios.h>
#include "smsc95xx.h"

/** @file
 *
 * SMSC LAN95xx USB Ethernet driver
 *
 */

/** Bulk IN completion profiler */
static struct profiler smsc95xx_in_profiler __profiler =
	{ .name = "smsc95xx.in" };

/** Bulk OUT profiler */
static struct profiler smsc95xx_out_profiler __profiler =
	{ .name = "smsc95xx.out" };

/******************************************************************************
 *
 * MAC address
 *
 ******************************************************************************
 */

/**
 * Construct MAC address for Honeywell VM3
 *
 * @v smscusb		SMSC USB device
 * @ret rc		Return status code
 */
static int smsc95xx_vm3_fetch_mac ( struct smscusb_device *smscusb ) {
	struct net_device *netdev = smscusb->netdev;
	struct smbios_structure structure;
	struct smbios_system_information system;
	struct {
		char manufacturer[ 10 /* "Honeywell" + NUL */ ];
		char product[ 4 /* "VM3" + NUL */ ];
		char mac[ base16_encoded_len ( ETH_ALEN ) + 1 /* NUL */ ];
	} strings;
	int len;
	int rc;

	/* Find system information */
	if ( ( rc = find_smbios_structure ( SMBIOS_TYPE_SYSTEM_INFORMATION, 0,
					    &structure ) ) != 0 ) {
		DBGC ( smscusb, "SMSC95XX %p could not find system "
		       "information: %s\n", smscusb, strerror ( rc ) );
		return rc;
	}

	/* Read system information */
	if ( ( rc = read_smbios_structure ( &structure, &system,
					    sizeof ( system ) ) ) != 0 ) {
		DBGC ( smscusb, "SMSC95XX %p could not read system "
		       "information: %s\n", smscusb, strerror ( rc ) );
		return rc;
	}

	/* NUL-terminate all strings to be fetched */
	memset ( &strings, 0, sizeof ( strings ) );

	/* Fetch system manufacturer name */
	len = read_smbios_string ( &structure, system.manufacturer,
				   strings.manufacturer,
				   ( sizeof ( strings.manufacturer ) - 1 ) );
	if ( len < 0 ) {
		rc = len;
		DBGC ( smscusb, "SMSC95XX %p could not read manufacturer "
		       "name: %s\n", smscusb, strerror ( rc ) );
		return rc;
	}

	/* Fetch system product name */
	len = read_smbios_string ( &structure, system.product, strings.product,
				   ( sizeof ( strings.product ) - 1 ) );
	if ( len < 0 ) {
		rc = len;
		DBGC ( smscusb, "SMSC95XX %p could not read product name: "
		       "%s\n", smscusb, strerror ( rc ) );
		return rc;
	}

	/* Ignore non-VM3 devices */
	if ( ( strcmp ( strings.manufacturer, "Honeywell" ) != 0 ) ||
	     ( strcmp ( strings.product, "VM3" ) != 0 ) )
		return -ENOTTY;

	/* Find OEM strings */
	if ( ( rc = find_smbios_structure ( SMBIOS_TYPE_OEM_STRINGS, 0,
					    &structure ) ) != 0 ) {
		DBGC ( smscusb, "SMSC95XX %p could not find OEM strings: %s\n",
		       smscusb, strerror ( rc ) );
		return rc;
	}

	/* Fetch MAC address */
	len = read_smbios_string ( &structure, SMSC95XX_VM3_OEM_STRING_MAC,
				   strings.mac, ( sizeof ( strings.mac ) - 1 ));
	if ( len < 0 ) {
		rc = len;
		DBGC ( smscusb, "SMSC95XX %p could not read OEM string: %s\n",
		       smscusb, strerror ( rc ) );
		return rc;
	}

	/* Sanity check */
	if ( len != ( ( int ) ( sizeof ( strings.mac ) - 1 ) ) ) {
		DBGC ( smscusb, "SMSC95XX %p invalid MAC address \"%s\"\n",
		       smscusb, strings.mac );
		return -EINVAL;
	}

	/* Decode MAC address */
	len = base16_decode ( strings.mac, netdev->hw_addr, ETH_ALEN );
	if ( len < 0 ) {
		rc = len;
		DBGC ( smscusb, "SMSC95XX %p invalid MAC address \"%s\"\n",
		       smscusb, strings.mac );
		return rc;
	}

	DBGC ( smscusb, "SMSC95XX %p using VM3 MAC %s\n",
	       smscusb, eth_ntoa ( netdev->hw_addr ) );
	return 0;
}

/**
 * Fetch MAC address
 *
 * @v smscusb		SMSC USB device
 * @ret rc		Return status code
 */
static int smsc95xx_fetch_mac ( struct smscusb_device *smscusb ) {
	struct net_device *netdev = smscusb->netdev;
	int rc;

	/* Read MAC address from EEPROM, if present */
	if ( ( rc = smscusb_eeprom_fetch_mac ( smscusb,
					       SMSC95XX_E2P_BASE ) ) == 0 )
		return 0;

	/* Construct MAC address for Honeywell VM3, if applicable */
	if ( ( rc = smsc95xx_vm3_fetch_mac ( smscusb ) ) == 0 )
		return 0;

	/* Otherwise, generate a random MAC address */
	eth_random_addr ( netdev->hw_addr );
	DBGC ( smscusb, "SMSC95XX %p using random MAC %s\n",
	       smscusb, eth_ntoa ( netdev->hw_addr ) );
	return 0;
}

/******************************************************************************
 *
 * Statistics (for debugging)
 *
 ******************************************************************************
 */

/**
 * Dump statistics (for debugging)
 *
 * @v smscusb		SMSC USB device
 * @ret rc		Return status code
 */
static int smsc95xx_dump_statistics ( struct smscusb_device *smscusb ) {
	struct smsc95xx_rx_statistics rx;
	struct smsc95xx_tx_statistics tx;
	int rc;

	/* Do nothing unless debugging is enabled */
	if ( ! DBG_LOG )
		return 0;

	/* Get RX statistics */
	if ( ( rc = smscusb_get_statistics ( smscusb, SMSC95XX_RX_STATISTICS,
					     &rx, sizeof ( rx ) ) ) != 0 ) {
		DBGC ( smscusb, "SMSC95XX %p could not get RX statistics: "
		       "%s\n", smscusb, strerror ( rc ) );
		return rc;
	}

	/* Get TX statistics */
	if ( ( rc = smscusb_get_statistics ( smscusb, SMSC95XX_TX_STATISTICS,
					     &tx, sizeof ( tx ) ) ) != 0 ) {
		DBGC ( smscusb, "SMSC95XX %p could not get TX statistics: "
		       "%s\n", smscusb, strerror ( rc ) );
		return rc;
	}

	/* Dump statistics */
	DBGC ( smscusb, "SMSC95XX %p RX good %d bad %d crc %d und %d aln %d "
	       "ovr %d lat %d drp %d\n", smscusb, le32_to_cpu ( rx.good ),
	       le32_to_cpu ( rx.bad ), le32_to_cpu ( rx.crc ),
	       le32_to_cpu ( rx.undersize ), le32_to_cpu ( rx.alignment ),
	       le32_to_cpu ( rx.oversize ), le32_to_cpu ( rx.late ),
	       le32_to_cpu ( rx.dropped ) );
	DBGC ( smscusb, "SMSC95XX %p TX good %d bad %d pau %d sgl %d mul %d "
	       "exc %d lat %d und %d def %d car %d\n", smscusb,
	       le32_to_cpu ( tx.good ), le32_to_cpu ( tx.bad ),
	       le32_to_cpu ( tx.pause ), le32_to_cpu ( tx.single ),
	       le32_to_cpu ( tx.multiple ), le32_to_cpu ( tx.excessive ),
	       le32_to_cpu ( tx.late ), le32_to_cpu ( tx.underrun ),
	       le32_to_cpu ( tx.deferred ), le32_to_cpu ( tx.carrier ) );

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
static int smsc95xx_reset ( struct smscusb_device *smscusb ) {
	uint32_t hw_cfg;
	uint32_t led_gpio_cfg;
	int rc;

	/* Reset device */
	if ( ( rc = smscusb_writel ( smscusb, SMSC95XX_HW_CFG,
				     SMSC95XX_HW_CFG_LRST ) ) != 0 )
		return rc;

	/* Wait for reset to complete */
	udelay ( SMSC95XX_RESET_DELAY_US );

	/* Check that reset has completed */
	if ( ( rc = smscusb_readl ( smscusb, SMSC95XX_HW_CFG, &hw_cfg ) ) != 0 )
		return rc;
	if ( hw_cfg & SMSC95XX_HW_CFG_LRST ) {
		DBGC ( smscusb, "SMSC95XX %p failed to reset\n", smscusb );
		return -ETIMEDOUT;
	}

	/* Configure LEDs */
	led_gpio_cfg = ( SMSC95XX_LED_GPIO_CFG_GPCTL2_NSPD_LED |
			 SMSC95XX_LED_GPIO_CFG_GPCTL1_NLNKA_LED |
			 SMSC95XX_LED_GPIO_CFG_GPCTL0_NFDX_LED );
	if ( ( rc = smscusb_writel ( smscusb, SMSC95XX_LED_GPIO_CFG,
				     led_gpio_cfg ) ) != 0 ) {
		DBGC ( smscusb, "SMSC95XX %p could not configure LEDs: %s\n",
		       smscusb, strerror ( rc ) );
		/* Ignore error and continue */
	}

	return 0;
}

/******************************************************************************
 *
 * Endpoint operations
 *
 ******************************************************************************
 */

/**
 * Complete bulk IN transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void smsc95xx_in_complete ( struct usb_endpoint *ep,
				   struct io_buffer *iobuf, int rc ) {
	struct smscusb_device *smscusb =
		container_of ( ep, struct smscusb_device, usbnet.in );
	struct net_device *netdev = smscusb->netdev;
	struct smsc95xx_rx_header *header;

	/* Profile completions */
	profile_start ( &smsc95xx_in_profiler );

	/* Ignore packets cancelled when the endpoint closes */
	if ( ! ep->open ) {
		free_iob ( iobuf );
		return;
	}

	/* Record USB errors against the network device */
	if ( rc != 0 ) {
		DBGC ( smscusb, "SMSC95XX %p bulk IN failed: %s\n",
		       smscusb, strerror ( rc ) );
		goto err;
	}

	/* Sanity check */
	if ( iob_len ( iobuf ) < ( sizeof ( *header ) + 4 /* CRC */ ) ) {
		DBGC ( smscusb, "SMSC95XX %p underlength bulk IN\n",
		       smscusb );
		DBGC_HDA ( smscusb, 0, iobuf->data, iob_len ( iobuf ) );
		rc = -EINVAL;
		goto err;
	}

	/* Strip header and CRC */
	header = iobuf->data;
	iob_pull ( iobuf, sizeof ( *header ) );
	iob_unput ( iobuf, 4 /* CRC */ );

	/* Check for errors */
	if ( header->command & cpu_to_le32 ( SMSC95XX_RX_RUNT |
					     SMSC95XX_RX_LATE |
					     SMSC95XX_RX_CRC ) ) {
		DBGC ( smscusb, "SMSC95XX %p receive error (%08x):\n",
		       smscusb, le32_to_cpu ( header->command ) );
		DBGC_HDA ( smscusb, 0, iobuf->data, iob_len ( iobuf ) );
		rc = -EIO;
		goto err;
	}

	/* Hand off to network stack */
	netdev_rx ( netdev, iob_disown ( iobuf ) );

	profile_stop ( &smsc95xx_in_profiler );
	return;

 err:
	/* Hand off to network stack */
	netdev_rx_err ( netdev, iob_disown ( iobuf ), rc );
}

/** Bulk IN endpoint operations */
static struct usb_endpoint_driver_operations smsc95xx_in_operations = {
	.complete = smsc95xx_in_complete,
};

/**
 * Transmit packet
 *
 * @v smscusb		SMSC USB device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int smsc95xx_out_transmit ( struct smscusb_device *smscusb,
				   struct io_buffer *iobuf ) {
	struct smsc95xx_tx_header *header;
	size_t len = iob_len ( iobuf );
	int rc;

	/* Profile transmissions */
	profile_start ( &smsc95xx_out_profiler );

	/* Prepend header */
	if ( ( rc = iob_ensure_headroom ( iobuf, sizeof ( *header ) ) ) != 0 )
		return rc;
	header = iob_push ( iobuf, sizeof ( *header ) );
	header->command = cpu_to_le32 ( SMSC95XX_TX_FIRST | SMSC95XX_TX_LAST |
					SMSC95XX_TX_LEN ( len ) );
	header->len = cpu_to_le32 ( len );

	/* Enqueue I/O buffer */
	if ( ( rc = usb_stream ( &smscusb->usbnet.out, iobuf, 0 ) ) != 0 )
		return rc;

	profile_stop ( &smsc95xx_out_profiler );
	return 0;
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
static int smsc95xx_open ( struct net_device *netdev ) {
	struct smscusb_device *smscusb = netdev->priv;
	int rc;

	/* Clear stored interrupt status */
	smscusb->int_sts = 0;

	/* Configure bulk IN empty response */
	if ( ( rc = smscusb_writel ( smscusb, SMSC95XX_HW_CFG,
				     SMSC95XX_HW_CFG_BIR ) ) != 0 )
		goto err_hw_cfg;

	/* Open USB network device */
	if ( ( rc = usbnet_open ( &smscusb->usbnet ) ) != 0 ) {
		DBGC ( smscusb, "SMSC95XX %p could not open: %s\n",
		       smscusb, strerror ( rc ) );
		goto err_open;
	}

	/* Configure interrupt endpoint */
	if ( ( rc = smscusb_writel ( smscusb, SMSC95XX_INT_EP_CTL,
				     ( SMSC95XX_INT_EP_CTL_RXDF_EN |
				       SMSC95XX_INT_EP_CTL_PHY_EN ) ) ) != 0 )
		goto err_int_ep_ctl;

	/* Configure bulk IN delay */
	if ( ( rc = smscusb_writel ( smscusb, SMSC95XX_BULK_IN_DLY,
				     SMSC95XX_BULK_IN_DLY_SET ( 0 ) ) ) != 0 )
		goto err_bulk_in_dly;

	/* Configure MAC */
	if ( ( rc = smscusb_writel ( smscusb, SMSC95XX_MAC_CR,
				     ( SMSC95XX_MAC_CR_RXALL |
				       SMSC95XX_MAC_CR_FDPX |
				       SMSC95XX_MAC_CR_MCPAS |
				       SMSC95XX_MAC_CR_PRMS |
				       SMSC95XX_MAC_CR_PASSBAD |
				       SMSC95XX_MAC_CR_TXEN |
				       SMSC95XX_MAC_CR_RXEN ) ) ) != 0 )
		goto err_mac_cr;

	/* Configure transmit datapath */
	if ( ( rc = smscusb_writel ( smscusb, SMSC95XX_TX_CFG,
				     SMSC95XX_TX_CFG_ON ) ) != 0 )
		goto err_tx_cfg;

	/* Set MAC address */
	if ( ( rc = smscusb_set_address ( smscusb, SMSC95XX_ADDR_BASE ) ) != 0 )
		goto err_set_address;

	/* Enable PHY interrupts and update link status */
	if ( ( rc = smscusb_mii_open ( smscusb, SMSC95XX_MII_PHY_INTR_MASK,
				       ( SMSC95XX_PHY_INTR_ANEG_DONE |
					 SMSC95XX_PHY_INTR_LINK_DOWN ) ) ) != 0)
		goto err_mii_open;

	return 0;

 err_mii_open:
 err_set_address:
 err_tx_cfg:
 err_mac_cr:
 err_bulk_in_dly:
 err_int_ep_ctl:
	usbnet_close ( &smscusb->usbnet );
 err_open:
 err_hw_cfg:
	smsc95xx_reset ( smscusb );
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void smsc95xx_close ( struct net_device *netdev ) {
	struct smscusb_device *smscusb = netdev->priv;

	/* Close USB network device */
	usbnet_close ( &smscusb->usbnet );

	/* Dump statistics (for debugging) */
	smsc95xx_dump_statistics ( smscusb );

	/* Reset device */
	smsc95xx_reset ( smscusb );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int smsc95xx_transmit ( struct net_device *netdev,
			       struct io_buffer *iobuf ) {
	struct smscusb_device *smscusb = netdev->priv;
	int rc;

	/* Transmit packet */
	if ( ( rc = smsc95xx_out_transmit ( smscusb, iobuf ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void smsc95xx_poll ( struct net_device *netdev ) {
	struct smscusb_device *smscusb = netdev->priv;
	uint32_t int_sts;
	int rc;

	/* Poll USB bus */
	usb_poll ( smscusb->bus );

	/* Refill endpoints */
	if ( ( rc = usbnet_refill ( &smscusb->usbnet ) ) != 0 )
		netdev_rx_err ( netdev, NULL, rc );

	/* Do nothing more unless there are interrupts to handle */
	int_sts = smscusb->int_sts;
	if ( ! int_sts )
		return;

	/* Check link status if applicable */
	if ( int_sts & SMSC95XX_INT_STS_PHY_INT ) {
		smscusb_mii_check_link ( smscusb );
		int_sts &= ~SMSC95XX_INT_STS_PHY_INT;
	}

	/* Record RX FIFO overflow if applicable */
	if ( int_sts & SMSC95XX_INT_STS_RXDF_INT ) {
		DBGC2 ( smscusb, "SMSC95XX %p RX FIFO overflowed\n",
			smscusb );
		netdev_rx_err ( netdev, NULL, -ENOBUFS );
		int_sts &= ~SMSC95XX_INT_STS_RXDF_INT;
	}

	/* Check for unexpected interrupts */
	if ( int_sts ) {
		DBGC ( smscusb, "SMSC95XX %p unexpected interrupt %#08x\n",
		       smscusb, int_sts );
		netdev_rx_err ( netdev, NULL, -ENOTTY );
	}

	/* Clear interrupts */
	if ( ( rc = smscusb_writel ( smscusb, SMSC95XX_INT_STS,
				     smscusb->int_sts ) ) != 0 )
		netdev_rx_err ( netdev, NULL, rc );
	smscusb->int_sts = 0;
}

/** SMSC95xx network device operations */
static struct net_device_operations smsc95xx_operations = {
	.open		= smsc95xx_open,
	.close		= smsc95xx_close,
	.transmit	= smsc95xx_transmit,
	.poll		= smsc95xx_poll,
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
static int smsc95xx_probe ( struct usb_function *func,
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
	netdev_init ( netdev, &smsc95xx_operations );
	netdev->dev = &func->dev;
	smscusb = netdev->priv;
	memset ( smscusb, 0, sizeof ( *smscusb ) );
	smscusb_init ( smscusb, netdev, func, &smsc95xx_in_operations );
	smscusb_mii_init ( smscusb, SMSC95XX_MII_BASE,
			   SMSC95XX_MII_PHY_INTR_SOURCE );
	usb_refill_init ( &smscusb->usbnet.in,
			  ( sizeof ( struct smsc95xx_tx_header ) -
			    sizeof ( struct smsc95xx_rx_header ) ),
			  SMSC95XX_IN_MTU, SMSC95XX_IN_MAX_FILL );
	DBGC ( smscusb, "SMSC95XX %p on %s\n", smscusb, func->name );

	/* Describe USB network device */
	if ( ( rc = usbnet_describe ( &smscusb->usbnet, config ) ) != 0 ) {
		DBGC ( smscusb, "SMSC95XX %p could not describe: %s\n",
		       smscusb, strerror ( rc ) );
		goto err_describe;
	}

	/* Reset device */
	if ( ( rc = smsc95xx_reset ( smscusb ) ) != 0 )
		goto err_reset;

	/* Read MAC address */
	if ( ( rc = smsc95xx_fetch_mac ( smscusb ) ) != 0 )
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
static void smsc95xx_remove ( struct usb_function *func ) {
	struct net_device *netdev = usb_func_get_drvdata ( func );

	unregister_netdev ( netdev );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** SMSC95xx device IDs */
static struct usb_device_id smsc95xx_ids[] = {
	{
		.name = "smsc9500",
		.vendor = 0x0424,
		.product = 0x9500,
	},
	{
		.name = "smsc9505",
		.vendor = 0x0424,
		.product = 0x9505,
	},
	{
		.name = "smsc9500a",
		.vendor = 0x0424,
		.product = 0x9e00,
	},
	{
		.name = "smsc9505a",
		.vendor = 0x0424,
		.product = 0x9e01,
	},
	{
		.name = "smsc9514",
		.vendor = 0x0424,
		.product = 0xec00,
	},
	{
		.name = "smsc9500-s",
		.vendor = 0x0424,
		.product = 0x9900,
	},
	{
		.name = "smsc9505-s",
		.vendor = 0x0424,
		.product = 0x9901,
	},
	{
		.name = "smsc9500a-s",
		.vendor = 0x0424,
		.product = 0x9902,
	},
	{
		.name = "smsc9505a-s",
		.vendor = 0x0424,
		.product = 0x9903,
	},
	{
		.name = "smsc9514-s",
		.vendor = 0x0424,
		.product = 0x9904,
	},
	{
		.name = "smsc9500a-h",
		.vendor = 0x0424,
		.product = 0x9905,
	},
	{
		.name = "smsc9505a-h",
		.vendor = 0x0424,
		.product = 0x9906,
	},
	{
		.name = "smsc9500-2",
		.vendor = 0x0424,
		.product = 0x9907,
	},
	{
		.name = "smsc9500a-2",
		.vendor = 0x0424,
		.product = 0x9908,
	},
	{
		.name = "smsc9514-2",
		.vendor = 0x0424,
		.product = 0x9909,
	},
	{
		.name = "smsc9530",
		.vendor = 0x0424,
		.product = 0x9530,
	},
	{
		.name = "smsc9730",
		.vendor = 0x0424,
		.product = 0x9730,
	},
	{
		.name = "smsc89530",
		.vendor = 0x0424,
		.product = 0x9e08,
	},
};

/** SMSC LAN95xx driver */
struct usb_driver smsc95xx_driver __usb_driver = {
	.ids = smsc95xx_ids,
	.id_count = ( sizeof ( smsc95xx_ids ) / sizeof ( smsc95xx_ids[0] ) ),
	.class = USB_CLASS_ID ( 0xff, 0x00, 0xff ),
	.score = USB_SCORE_NORMAL,
	.probe = smsc95xx_probe,
	.remove = smsc95xx_remove,
};
