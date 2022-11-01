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
#include "smsc75xx.h"

/** @file
 *
 * SMSC LAN75xx USB Ethernet driver
 *
 */

/** Bulk IN completion profiler */
static struct profiler smsc75xx_in_profiler __profiler =
	{ .name = "smsc75xx.in" };

/** Bulk OUT profiler */
static struct profiler smsc75xx_out_profiler __profiler =
	{ .name = "smsc75xx.out" };

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
int smsc75xx_dump_statistics ( struct smscusb_device *smscusb ) {
	struct smsc75xx_statistics stats;
	int rc;

	/* Do nothing unless debugging is enabled */
	if ( ! DBG_LOG )
		return 0;

	/* Get statistics */
	if ( ( rc = smscusb_get_statistics ( smscusb, 0, &stats,
					     sizeof ( stats ) ) ) != 0 ) {
		DBGC ( smscusb, "SMSC75XX %p could not get statistics: "
		       "%s\n", smscusb, strerror ( rc ) );
		return rc;
	}

	/* Dump statistics */
	DBGC ( smscusb, "SMSC75XX %p RXE fcs %d aln %d frg %d jab %d und %d "
	       "ovr %d drp %d\n", smscusb, le32_to_cpu ( stats.rx.err.fcs ),
	       le32_to_cpu ( stats.rx.err.alignment ),
	       le32_to_cpu ( stats.rx.err.fragment ),
	       le32_to_cpu ( stats.rx.err.jabber ),
	       le32_to_cpu ( stats.rx.err.undersize ),
	       le32_to_cpu ( stats.rx.err.oversize ),
	       le32_to_cpu ( stats.rx.err.dropped ) );
	DBGC ( smscusb, "SMSC75XX %p RXB ucast %d bcast %d mcast %d\n",
	       smscusb, le32_to_cpu ( stats.rx.byte.unicast ),
	       le32_to_cpu ( stats.rx.byte.broadcast ),
	       le32_to_cpu ( stats.rx.byte.multicast ) );
	DBGC ( smscusb, "SMSC75XX %p RXF ucast %d bcast %d mcast %d pause "
	       "%d\n", smscusb, le32_to_cpu ( stats.rx.frame.unicast ),
	       le32_to_cpu ( stats.rx.frame.broadcast ),
	       le32_to_cpu ( stats.rx.frame.multicast ),
	       le32_to_cpu ( stats.rx.frame.pause ) );
	DBGC ( smscusb, "SMSC75XX %p TXE fcs %d def %d car %d cnt %d sgl %d "
	       "mul %d exc %d lat %d\n", smscusb,
	       le32_to_cpu ( stats.tx.err.fcs ),
	       le32_to_cpu ( stats.tx.err.deferral ),
	       le32_to_cpu ( stats.tx.err.carrier ),
	       le32_to_cpu ( stats.tx.err.count ),
	       le32_to_cpu ( stats.tx.err.single ),
	       le32_to_cpu ( stats.tx.err.multiple ),
	       le32_to_cpu ( stats.tx.err.excessive ),
	       le32_to_cpu ( stats.tx.err.late ) );
	DBGC ( smscusb, "SMSC75XX %p TXB ucast %d bcast %d mcast %d\n",
	       smscusb, le32_to_cpu ( stats.tx.byte.unicast ),
	       le32_to_cpu ( stats.tx.byte.broadcast ),
	       le32_to_cpu ( stats.tx.byte.multicast ) );
	DBGC ( smscusb, "SMSC75XX %p TXF ucast %d bcast %d mcast %d pause "
	       "%d\n", smscusb, le32_to_cpu ( stats.tx.frame.unicast ),
	       le32_to_cpu ( stats.tx.frame.broadcast ),
	       le32_to_cpu ( stats.tx.frame.multicast ),
	       le32_to_cpu ( stats.tx.frame.pause ) );

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
static int smsc75xx_reset ( struct smscusb_device *smscusb ) {
	uint32_t hw_cfg;
	unsigned int i;
	int rc;

	/* Reset device */
	if ( ( rc = smscusb_writel ( smscusb, SMSC75XX_HW_CFG,
				     SMSC75XX_HW_CFG_LRST ) ) != 0 )
		return rc;

	/* Wait for reset to complete */
	for ( i = 0 ; i < SMSC75XX_RESET_MAX_WAIT_MS ; i++ ) {

		/* Check if reset has completed */
		if ( ( rc = smscusb_readl ( smscusb, SMSC75XX_HW_CFG,
					    &hw_cfg ) ) != 0 )
			return rc;
		if ( ! ( hw_cfg & SMSC75XX_HW_CFG_LRST ) )
			return 0;

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( smscusb, "SMSC75XX %p timed out waiting for reset\n",
	       smscusb );
	return -ETIMEDOUT;
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
static void smsc75xx_in_complete ( struct usb_endpoint *ep,
				   struct io_buffer *iobuf, int rc ) {
	struct smscusb_device *smscusb =
		container_of ( ep, struct smscusb_device, usbnet.in );
	struct net_device *netdev = smscusb->netdev;
	struct smsc75xx_rx_header *header;

	/* Profile completions */
	profile_start ( &smsc75xx_in_profiler );

	/* Ignore packets cancelled when the endpoint closes */
	if ( ! ep->open ) {
		free_iob ( iobuf );
		return;
	}

	/* Record USB errors against the network device */
	if ( rc != 0 ) {
		DBGC ( smscusb, "SMSC75XX %p bulk IN failed: %s\n",
		       smscusb, strerror ( rc ) );
		goto err;
	}

	/* Sanity check */
	if ( iob_len ( iobuf ) < ( sizeof ( *header ) ) ) {
		DBGC ( smscusb, "SMSC75XX %p underlength bulk IN\n",
		       smscusb );
		DBGC_HDA ( smscusb, 0, iobuf->data, iob_len ( iobuf ) );
		rc = -EINVAL;
		goto err;
	}

	/* Strip header */
	header = iobuf->data;
	iob_pull ( iobuf, sizeof ( *header ) );

	/* Check for errors */
	if ( header->command & cpu_to_le32 ( SMSC75XX_RX_RED ) ) {
		DBGC ( smscusb, "SMSC75XX %p receive error (%08x):\n",
		       smscusb, le32_to_cpu ( header->command ) );
		DBGC_HDA ( smscusb, 0, iobuf->data, iob_len ( iobuf ) );
		rc = -EIO;
		goto err;
	}

	/* Hand off to network stack */
	netdev_rx ( netdev, iob_disown ( iobuf ) );

	profile_stop ( &smsc75xx_in_profiler );
	return;

 err:
	/* Hand off to network stack */
	netdev_rx_err ( netdev, iob_disown ( iobuf ), rc );
}

/** Bulk IN endpoint operations */
struct usb_endpoint_driver_operations smsc75xx_in_operations = {
	.complete = smsc75xx_in_complete,
};

/**
 * Transmit packet
 *
 * @v smscusb		SMSC USB device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int smsc75xx_out_transmit ( struct smscusb_device *smscusb,
				   struct io_buffer *iobuf ) {
	struct smsc75xx_tx_header *header;
	size_t len = iob_len ( iobuf );
	int rc;

	/* Profile transmissions */
	profile_start ( &smsc75xx_out_profiler );

	/* Prepend header */
	if ( ( rc = iob_ensure_headroom ( iobuf, sizeof ( *header ) ) ) != 0 )
		return rc;
	header = iob_push ( iobuf, sizeof ( *header ) );
	header->command = cpu_to_le32 ( SMSC75XX_TX_FCS | len );
	header->tag = 0;
	header->mss = 0;

	/* Enqueue I/O buffer */
	if ( ( rc = usb_stream ( &smscusb->usbnet.out, iobuf, 0 ) ) != 0 )
		return rc;

	profile_stop ( &smsc75xx_out_profiler );
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
static int smsc75xx_open ( struct net_device *netdev ) {
	struct smscusb_device *smscusb = netdev->priv;
	int rc;

	/* Clear stored interrupt status */
	smscusb->int_sts = 0;

	/* Configure bulk IN empty response */
	if ( ( rc = smscusb_writel ( smscusb, SMSC75XX_HW_CFG,
				     SMSC75XX_HW_CFG_BIR ) ) != 0 )
		goto err_hw_cfg;

	/* Open USB network device */
	if ( ( rc = usbnet_open ( &smscusb->usbnet ) ) != 0 ) {
		DBGC ( smscusb, "SMSC75XX %p could not open: %s\n",
		       smscusb, strerror ( rc ) );
		goto err_open;
	}

	/* Configure interrupt endpoint */
	if ( ( rc = smscusb_writel ( smscusb, SMSC75XX_INT_EP_CTL,
				     ( SMSC75XX_INT_EP_CTL_RDFO_EN |
				       SMSC75XX_INT_EP_CTL_PHY_EN ) ) ) != 0 )
		goto err_int_ep_ctl;

	/* Configure bulk IN delay */
	if ( ( rc = smscusb_writel ( smscusb, SMSC75XX_BULK_IN_DLY,
				     SMSC75XX_BULK_IN_DLY_SET ( 0 ) ) ) != 0 )
		goto err_bulk_in_dly;

	/* Configure receive filters */
	if ( ( rc = smscusb_writel ( smscusb, SMSC75XX_RFE_CTL,
				     ( SMSC75XX_RFE_CTL_AB |
				       SMSC75XX_RFE_CTL_AM |
				       SMSC75XX_RFE_CTL_AU ) ) ) != 0 )
		goto err_rfe_ctl;

	/* Configure receive FIFO */
	if ( ( rc = smscusb_writel ( smscusb, SMSC75XX_FCT_RX_CTL,
				     ( SMSC75XX_FCT_RX_CTL_EN |
				       SMSC75XX_FCT_RX_CTL_BAD ) ) ) != 0 )
		goto err_fct_rx_ctl;

	/* Configure transmit FIFO */
	if ( ( rc = smscusb_writel ( smscusb, SMSC75XX_FCT_TX_CTL,
				     SMSC75XX_FCT_TX_CTL_EN ) ) != 0 )
		goto err_fct_tx_ctl;

	/* Configure receive datapath */
	if ( ( rc = smscusb_writel ( smscusb, SMSC75XX_MAC_RX,
				     ( SMSC75XX_MAC_RX_MAX_SIZE_DEFAULT |
				       SMSC75XX_MAC_RX_FCS |
				       SMSC75XX_MAC_RX_EN ) ) ) != 0 )
		goto err_mac_rx;

	/* Configure transmit datapath */
	if ( ( rc = smscusb_writel ( smscusb, SMSC75XX_MAC_TX,
				     SMSC75XX_MAC_TX_EN ) ) != 0 )
		goto err_mac_tx;

	/* Set MAC address */
	if ( ( rc = smscusb_set_address ( smscusb,
					  SMSC75XX_RX_ADDR_BASE ) ) != 0 )
		goto err_set_address;

	/* Set MAC address perfect filter */
	if ( ( rc = smscusb_set_filter ( smscusb,
					 SMSC75XX_ADDR_FILT_BASE ) ) != 0 )
		goto err_set_filter;

	/* Enable PHY interrupts and update link status */
	if ( ( rc = smscusb_mii_open ( smscusb, SMSC75XX_MII_PHY_INTR_MASK,
				       ( SMSC75XX_PHY_INTR_ANEG_DONE |
					 SMSC75XX_PHY_INTR_LINK_DOWN ) ) ) != 0)
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
 err_hw_cfg:
	smsc75xx_reset ( smscusb );
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void smsc75xx_close ( struct net_device *netdev ) {
	struct smscusb_device *smscusb = netdev->priv;

	/* Close USB network device */
	usbnet_close ( &smscusb->usbnet );

	/* Dump statistics (for debugging) */
	if ( DBG_LOG )
		smsc75xx_dump_statistics ( smscusb );

	/* Reset device */
	smsc75xx_reset ( smscusb );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
int smsc75xx_transmit ( struct net_device *netdev, struct io_buffer *iobuf ) {
	struct smscusb_device *smscusb = netdev->priv;
	int rc;

	/* Transmit packet */
	if ( ( rc = smsc75xx_out_transmit ( smscusb, iobuf ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
void smsc75xx_poll ( struct net_device *netdev ) {
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
	if ( int_sts & SMSC75XX_INT_STS_PHY_INT ) {
		smscusb_mii_check_link ( smscusb );
		int_sts &= ~SMSC75XX_INT_STS_PHY_INT;
	}

	/* Record RX FIFO overflow if applicable */
	if ( int_sts & SMSC75XX_INT_STS_RDFO_INT ) {
		DBGC2 ( smscusb, "SMSC75XX %p RX FIFO overflowed\n", smscusb );
		netdev_rx_err ( netdev, NULL, -ENOBUFS );
		int_sts &= ~SMSC75XX_INT_STS_RDFO_INT;
	}

	/* Check for unexpected interrupts */
	if ( int_sts ) {
		DBGC ( smscusb, "SMSC75XX %p unexpected interrupt %#08x\n",
		       smscusb, int_sts );
		netdev_rx_err ( netdev, NULL, -ENOTTY );
	}

	/* Clear interrupts */
	if ( ( rc = smscusb_writel ( smscusb, SMSC75XX_INT_STS,
				     smscusb->int_sts ) ) != 0 )
		netdev_rx_err ( netdev, NULL, rc );
	smscusb->int_sts = 0;
}

/** SMSC75xx network device operations */
static struct net_device_operations smsc75xx_operations = {
	.open		= smsc75xx_open,
	.close		= smsc75xx_close,
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
static int smsc75xx_probe ( struct usb_function *func,
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
	netdev_init ( netdev, &smsc75xx_operations );
	netdev->dev = &func->dev;
	smscusb = netdev->priv;
	memset ( smscusb, 0, sizeof ( *smscusb ) );
	smscusb_init ( smscusb, netdev, func, &smsc75xx_in_operations );
	smscusb_mii_init ( smscusb, SMSC75XX_MII_BASE,
			   SMSC75XX_MII_PHY_INTR_SOURCE );
	usb_refill_init ( &smscusb->usbnet.in, 0, SMSC75XX_IN_MTU,
			  SMSC75XX_IN_MAX_FILL );
	DBGC ( smscusb, "SMSC75XX %p on %s\n", smscusb, func->name );

	/* Describe USB network device */
	if ( ( rc = usbnet_describe ( &smscusb->usbnet, config ) ) != 0 ) {
		DBGC ( smscusb, "SMSC75XX %p could not describe: %s\n",
		       smscusb, strerror ( rc ) );
		goto err_describe;
	}

	/* Reset device */
	if ( ( rc = smsc75xx_reset ( smscusb ) ) != 0 )
		goto err_reset;

	/* Read MAC address */
	if ( ( rc = smscusb_eeprom_fetch_mac ( smscusb,
					       SMSC75XX_E2P_BASE ) ) != 0 )
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
static void smsc75xx_remove ( struct usb_function *func ) {
	struct net_device *netdev = usb_func_get_drvdata ( func );

	unregister_netdev ( netdev );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** SMSC75xx device IDs */
static struct usb_device_id smsc75xx_ids[] = {
	{
		.name = "smsc7500",
		.vendor = 0x0424,
		.product = 0x7500,
	},
	{
		.name = "smsc7505",
		.vendor = 0x0424,
		.product = 0x7505,
	},
};

/** SMSC LAN75xx driver */
struct usb_driver smsc75xx_driver __usb_driver = {
	.ids = smsc75xx_ids,
	.id_count = ( sizeof ( smsc75xx_ids ) / sizeof ( smsc75xx_ids[0] ) ),
	.class = USB_CLASS_ID ( 0xff, 0x00, 0xff ),
	.score = USB_SCORE_NORMAL,
	.probe = smsc75xx_probe,
	.remove = smsc75xx_remove,
};
