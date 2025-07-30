/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/iobuf.h>
#include <ipxe/timer.h>
#include <ipxe/devtree.h>
#include <ipxe/fdt.h>
#include "dwmac.h"

/** @file
 *
 * Synopsys DesignWare MAC network driver
 *
 */

/******************************************************************************
 *
 * Debug
 *
 ******************************************************************************
 */

/**
 * Dump MAC registers (for debugging)
 *
 * @v dwmac		DesignWare MAC device
 */
static void dwmac_dump_mac ( struct dwmac *dwmac ) {

	/* Do nothing unless debugging is enabled */
	if ( ! DBG_LOG )
		return;

	/* Dump MAC registers */
	DBGC ( dwmac, "DWMAC %s ver %08x cfg %08x flt %08x flo %08x\n",
	       dwmac->name, readl ( dwmac->regs + DWMAC_VER ),
	       readl ( dwmac->regs + DWMAC_CFG ),
	       readl ( dwmac->regs + DWMAC_FILTER ),
	       readl ( dwmac->regs + DWMAC_FLOW ) );
	DBGC ( dwmac, "DWMAC %s isr %08x dbg %08x gmi %08x\n",
	       dwmac->name, readl ( dwmac->regs + DWMAC_ISR ),
	       readl ( dwmac->regs + DWMAC_DEBUG ),
	       readl ( dwmac->regs + DWMAC_GMII ) );
}

/**
 * Dump DMA registers (for debugging)
 *
 * @v dwmac		DesignWare MAC device
 */
static void dwmac_dump_dma ( struct dwmac *dwmac ) {
	uint32_t status;

	/* Do nothing unless debugging is enabled */
	if ( ! DBG_LOG )
		return;

	/* Dump DMA registers */
	status = readl ( dwmac->regs + DWMAC_STATUS );
	DBGC ( dwmac, "DWMAC %s bus %08x fea %08x axi %08x ahb %08x\n",
	       dwmac->name, readl ( dwmac->regs + DWMAC_BUS ),
	       readl ( dwmac->regs + DWMAC_FEATURE ),
	       readl ( dwmac->regs + DWMAC_AXI ),
	       readl ( dwmac->regs + DWMAC_AHB ) );
	DBGC ( dwmac, "DWMAC %s opm %08x sta %08x drp %08x\n",
	       dwmac->name, readl ( dwmac->regs + DWMAC_OP ),
	       status, readl ( dwmac->regs + DWMAC_DROP ) );
	DBGC ( dwmac, "DWMAC %s txb %08x txd %08x txb %08x\n",
	       dwmac->name, readl ( dwmac->regs + DWMAC_TXBASE ),
	       readl ( dwmac->regs + DWMAC_TXDESC ),
	       readl ( dwmac->regs + DWMAC_TXBUF ) );
	DBGC ( dwmac, "DWMAC %s rxb %08x rxd %08x rxb %08x\n",
	       dwmac->name, readl ( dwmac->regs + DWMAC_RXBASE ),
	       readl ( dwmac->regs + DWMAC_RXDESC ),
	       readl ( dwmac->regs + DWMAC_RXBUF ) );

	/* Clear sticky bits in status register, since nothing else will */
	writel ( status, ( dwmac->regs + DWMAC_STATUS ) );
}

/**
 * Dump all registers (for debugging)
 *
 * @v dwmac		DesignWare MAC device
 */
static void __attribute__ (( unused )) dwmac_dump ( struct dwmac *dwmac ) {

	/* Dump MAC and DMA registers */
	dwmac_dump_mac ( dwmac );
	dwmac_dump_dma ( dwmac );
}

/******************************************************************************
 *
 * Device reset
 *
 ******************************************************************************
 */

/**
 * Reset hardware
 *
 * @v dwmac		DesignWare MAC device
 * @ret rc		Return status code
 */
static int dwmac_reset ( struct dwmac *dwmac ) {
	unsigned int i;
	uint32_t bus;

	/* Trigger software reset */
	writel ( DWMAC_BUS_SWR, ( dwmac->regs + DWMAC_BUS ) );

	/* Wait for reset to complete */
	for ( i = 0 ; i < DWMAC_RESET_MAX_WAIT_MS ; i++ ) {

		/* Delay */
		mdelay ( 1 );

		/* Check for reset completion */
		bus = readl ( dwmac->regs + DWMAC_BUS );
		if ( ! ( bus & DWMAC_BUS_SWR ) )
			return 0;
	}

	DBGC ( dwmac, "DWMAC %s timed out waiting for reset\n",
	       dwmac->name );
	return -ETIMEDOUT;
}

/******************************************************************************
 *
 * Link state
 *
 ******************************************************************************
 */

/**
 * Check link state
 *
 * @v netdev		Network device
 */
static void dwmac_check_link ( struct net_device *netdev ) {
	struct dwmac *dwmac = netdev->priv;
	uint32_t gmii;

	/* Read SGMII/RGMII link status */
	gmii = readl ( dwmac->regs + DWMAC_GMII );
	DBGC ( dwmac, "DWMAC %s GMII link status %#08x\n", dwmac->name, gmii );

	/* Update network device */
	if ( gmii & DWMAC_GMII_LINK ) {
		netdev_link_up ( netdev );
	} else {
		netdev_link_down ( netdev );
	}
}

/******************************************************************************
 *
 * Network device interface
 *
 ******************************************************************************
 */

/**
 * Create descriptor ring
 *
 * @v dwmac		DesignWare MAC device
 * @v ring		Descriptor ring
 * @ret rc		Return status code
 */
static int dwmac_create_ring ( struct dwmac *dwmac, struct dwmac_ring *ring ) {
	struct dwmac_descriptor *desc;
	struct dwmac_descriptor *next;
	physaddr_t base;
	unsigned int i;

	/* Allocate descriptor ring (on its own size) */
	ring->desc = dma_alloc ( dwmac->dma, &ring->map, ring->len, ring->len );
	if ( ! ring->desc )
		return -ENOMEM;

	/* Initialise descriptor ring */
	memset ( ring->desc, 0, ring->len );
	for ( i = 0 ; i < ring->count ; i++ ) {
		desc = &ring->desc[i];
		desc->size = cpu_to_le16 ( DWMAC_RX_LEN |
					   DWMAC_SIZE_RX_CHAIN );
		desc->ctrl = ring->ctrl;
		assert ( desc->ctrl & DWMAC_CTRL_CHAIN );
		next = &ring->desc[ ( i + 1 ) & ( ring->count - 1 ) ];
		desc->next = dma ( &ring->map, next );
	}
	wmb();

	/* Program ring address */
	base = dma ( &ring->map, ring->desc );
	assert ( base == ( ( uint32_t ) base ) );
	writel ( base, ( dwmac->regs + DWMAC_DMA + ring->qbase ) );

	DBGC ( dwmac, "DWMAC %s ring %02x is at [%08lx,%08lx)\n",
	       dwmac->name, ring->qbase, virt_to_phys ( ring->desc ),
	       ( virt_to_phys ( ring->desc ) + ring->len ) );
	return 0;
}

/**
 * Destroy descriptor ring
 *
 * @v dwmac		DesignWare MAC device
 * @v ring		Descriptor ring
 */
static void dwmac_destroy_ring ( struct dwmac *dwmac,
				 struct dwmac_ring *ring ) {

	/* Clear ring address */
	writel ( 0, ( dwmac->regs + DWMAC_DMA + ring->qbase ) );

	/* Free descriptor ring */
	dma_free ( &ring->map, ring->desc, ring->len );
	ring->desc = NULL;
	ring->prod = 0;
	ring->cons = 0;
}

/**
 * Refill receive descriptor ring
 *
 * @v dwmac		DesignWare MAC device
 */
static void dwmac_refill_rx ( struct dwmac *dwmac ) {
	struct dwmac_descriptor *rx;
	struct io_buffer *iobuf;
	unsigned int rx_idx;
	unsigned int refilled = 0;

	/* Refill ring */
	while ( ( dwmac->rx.prod - dwmac->rx.cons ) != DWMAC_NUM_RX_DESC ) {

		/* Allocate I/O buffer */
		iobuf = alloc_rx_iob ( DWMAC_RX_LEN, dwmac->dma );
		if ( ! iobuf ) {
			/* Wait for next refill */
			break;
		}

		/* Get next receive descriptor */
		rx_idx = ( dwmac->rx.prod++ % DWMAC_NUM_RX_DESC );
		rx = &dwmac->rx.desc[rx_idx];

		/* Populate receive descriptor */
		rx->addr = cpu_to_le32 ( iob_dma ( iobuf ) );
		wmb();
		rx->stat = cpu_to_le32 ( DWMAC_STAT_OWN );

		/* Record I/O buffer */
		assert ( dwmac->rx_iobuf[rx_idx] == NULL );
		dwmac->rx_iobuf[rx_idx] = iobuf;

		DBGC2 ( dwmac, "DWMAC %s RX %d is [%08lx,%08lx)\n",
			dwmac->name, rx_idx, virt_to_phys ( iobuf->data ),
			( virt_to_phys ( iobuf->data ) + DWMAC_RX_LEN ) );
		refilled++;
	}

	/* Trigger poll */
	if ( refilled ) {
		wmb();
		writel ( 0, ( dwmac->regs + DWMAC_RXPOLL ) );
	}
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int dwmac_open ( struct net_device *netdev ) {
	struct dwmac *dwmac = netdev->priv;
	union dwmac_mac mac;
	int rc;

	/* Create transmit descriptor ring */
	if ( ( rc = dwmac_create_ring ( dwmac, &dwmac->tx ) ) != 0 )
		goto err_create_tx;

	/* Create receive descriptor ring */
	if ( ( rc = dwmac_create_ring ( dwmac, &dwmac->rx ) ) != 0 )
		goto err_create_rx;

	/* Set MAC address */
	memcpy ( mac.raw, netdev->ll_addr, ETH_ALEN );
	writel ( mac.reg.addrl, ( dwmac->regs + DWMAC_ADDRL ) );
	writel ( mac.reg.addrh, ( dwmac->regs + DWMAC_ADDRH ) );

	/* Enable promiscuous mode */
	writel ( DWMAC_FILTER_PR, ( dwmac->regs + DWMAC_FILTER ) );

	/* Enable transmit and receive */
	writel ( ( DWMAC_OP_TXSF | DWMAC_OP_RXSF |
		   DWMAC_OP_TXEN | DWMAC_OP_RXEN ),
		 ( dwmac->regs + DWMAC_OP ) );
	writel ( ( DWMAC_CFG_DO | DWMAC_CFG_FD |
		   DWMAC_CFG_TXEN | DWMAC_CFG_RXEN ),
		 ( dwmac->regs + DWMAC_CFG ) );

	/* Refill receive descriptor ring */
	dwmac_refill_rx ( dwmac );

	/* Update link state */
	dwmac_check_link ( netdev );

	return 0;

	dwmac_destroy_ring ( dwmac, &dwmac->rx );
 err_create_rx:
	dwmac_destroy_ring ( dwmac, &dwmac->tx );
 err_create_tx:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void dwmac_close ( struct net_device *netdev ) {
	struct dwmac *dwmac = netdev->priv;
	unsigned int i;

	/* Reset NIC */
	dwmac_reset ( dwmac );

	/* Discard unused receive buffers */
	for ( i = 0 ; i < DWMAC_NUM_RX_DESC ; i++ ) {
		if ( dwmac->rx_iobuf[i] )
			free_rx_iob ( dwmac->rx_iobuf[i] );
		dwmac->rx_iobuf[i] = NULL;
	}

	/* Destroy receive descriptor ring */
	dwmac_destroy_ring ( dwmac, &dwmac->rx );

	/* Destroy transmit descriptor ring */
	dwmac_destroy_ring ( dwmac, &dwmac->tx );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int dwmac_transmit ( struct net_device *netdev,
			    struct io_buffer *iobuf ) {
	struct dwmac *dwmac = netdev->priv;
	struct dwmac_descriptor *tx;
	unsigned int tx_idx;

	/* Get next transmit descriptor */
	if ( ( dwmac->tx.prod - dwmac->tx.cons ) >= DWMAC_NUM_TX_DESC ) {
		DBGC ( dwmac, "DWMAC %s out of transmit descriptors\n",
		       dwmac->name );
		return -ENOBUFS;
	}
	tx_idx = ( dwmac->tx.prod % DWMAC_NUM_TX_DESC );
	tx = &dwmac->tx.desc[tx_idx];

	/* Update producer index */
	dwmac->tx.prod++;

	/* Populate transmit descriptor */
	tx->size = cpu_to_le16 ( iob_len ( iobuf ) );
	tx->addr = cpu_to_le32 ( iob_dma ( iobuf ) );
	wmb();
	tx->stat = cpu_to_le32 ( DWMAC_STAT_OWN | DWMAC_STAT_TX_LAST |
				 DWMAC_STAT_TX_FIRST | DWMAC_STAT_TX_CHAIN );
	wmb();

	/* Initiate transmission */
	writel ( 0, ( dwmac->regs + DWMAC_TXPOLL ) );

	DBGC2 ( dwmac, "DWMAC %s TX %d is [%08lx,%08lx)\n",
		dwmac->name, tx_idx, virt_to_phys ( iobuf->data ),
		( virt_to_phys ( iobuf->data ) + iob_len ( iobuf ) ) );
	return 0;
}

/**
 * Poll for completed packets
 *
 * @V netdev		Network device
 */
static void dwmac_poll_tx ( struct net_device *netdev ) {
	struct dwmac *dwmac = netdev->priv;
	struct dwmac_descriptor *tx;
	unsigned int tx_idx;

	/* Check for completed packets */
	while ( dwmac->tx.cons != dwmac->tx.prod ) {

		/* Get next transmit descriptor */
		tx_idx = ( dwmac->tx.cons % DWMAC_NUM_TX_DESC );
		tx = &dwmac->tx.desc[tx_idx];

		/* Stop if descriptor is still owned by hardware */
		if ( tx->stat & cpu_to_le32 ( DWMAC_STAT_OWN ) )
			return;
		dwmac->tx.cons++;

		/* Report completion */
		if ( tx->stat & cpu_to_le32 ( DWMAC_STAT_ERR ) ) {
			DBGC ( dwmac, "DWMAC %s TX %d error %#08x\n",
			       dwmac->name, tx_idx, le32_to_cpu ( tx->stat ) );
			dwmac_dump ( dwmac );
			netdev_tx_complete_next_err ( netdev, -EIO );
		} else {
			DBGC2 ( dwmac, "DWMAC %s TX %d complete\n",
				dwmac->name, tx_idx );
			netdev_tx_complete_next ( netdev );
		}
	}
}

/**
 * Poll for received packets
 *
 * @v netdev		Network device
 */
static void dwmac_poll_rx ( struct net_device *netdev ) {
	struct dwmac *dwmac = netdev->priv;
	struct dwmac_descriptor *rx;
	struct io_buffer *iobuf;
	unsigned int rx_idx;
	uint32_t stat;
	size_t len;

	/* Check for received packets */
	while ( dwmac->rx.cons != dwmac->rx.prod ) {

		/* Get next receive descriptor */
		rx_idx = ( dwmac->rx.cons % DWMAC_NUM_RX_DESC );
		rx = &dwmac->rx.desc[rx_idx];

		/* Stop if descriptor is still in use */
		if ( rx->stat & cpu_to_le32 ( DWMAC_STAT_OWN ) )
			return;
		dwmac->rx.cons++;

		/* Consume I/O buffer */
		iobuf = dwmac->rx_iobuf[rx_idx];
		assert ( iobuf != NULL );
		dwmac->rx_iobuf[rx_idx] = NULL;

		/* Hand off to network stack */
		stat = le32_to_cpu ( rx->stat );
		assert ( stat & DWMAC_STAT_RX_FIRST );
		assert ( stat & DWMAC_STAT_RX_LAST );
		if ( stat & DWMAC_STAT_ERR ) {
			DBGC ( dwmac, "DWMAC %s RX %d error %#08x\n",
			       dwmac->name, rx_idx, stat );
			dwmac_dump ( dwmac );
			netdev_rx_err ( netdev, iobuf, -EIO );
		} else {
			len = ( DWMAC_STAT_RX_LEN ( stat ) - 4 /* CRC */ );
			iob_put ( iobuf, len );
			DBGC2 ( dwmac, "DWMAC %s RX %d complete (length "
				"%zd)\n", dwmac->name, rx_idx, len );
			netdev_rx ( netdev, iobuf );
		}
	}
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void dwmac_poll ( struct net_device *netdev ) {
	struct dwmac *dwmac = netdev->priv;
	uint32_t status;

	/* Check for link status changes */
	status = readl ( dwmac->regs + DWMAC_STATUS );
	if ( status & DWMAC_STATUS_LINK )
		dwmac_check_link ( netdev );

	/* Poll for TX competions, if applicable */
	dwmac_poll_tx ( netdev );

	/* Poll for RX completions */
	dwmac_poll_rx ( netdev );

	/* Refill RX ring */
	dwmac_refill_rx ( dwmac );
}

/** DesignWare MAC network device operations */
static struct net_device_operations dwmac_operations = {
	.open		= dwmac_open,
	.close		= dwmac_close,
	.transmit	= dwmac_transmit,
	.poll		= dwmac_poll,
};

/******************************************************************************
 *
 * Devicetree interface
 *
 ******************************************************************************
 */

/**
 * Probe devicetree device
 *
 * @v dt		Devicetree device
 * @v offset		Starting node offset
 * @ret rc		Return status code
 */
static int dwmac_probe ( struct dt_device *dt, unsigned int offset ) {
	struct net_device *netdev;
	struct dwmac *dwmac;
	union dwmac_mac mac;
	uint32_t version;
	int rc;

	/* Allocate and initialise net device */
	netdev = alloc_etherdev ( sizeof ( *dwmac ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &dwmac_operations );
	dwmac = netdev->priv;
	dt_set_drvdata ( dt, netdev );
	netdev->dev = &dt->dev;
	netdev->dma = &dt->dma;
	memset ( dwmac, 0, sizeof ( *dwmac ) );
	dwmac->dma = &dt->dma;
	dwmac->name = netdev->dev->name;
	dwmac_init_ring ( &dwmac->tx, DWMAC_NUM_TX_DESC, DWMAC_TXBASE,
			  ( DWMAC_CTRL_TX_FIRST | DWMAC_CTRL_TX_LAST |
			    DWMAC_CTRL_CHAIN ) );
	dwmac_init_ring ( &dwmac->rx, DWMAC_NUM_RX_DESC, DWMAC_RXBASE,
			  DWMAC_CTRL_CHAIN );

	/* Map registers */
	dwmac->regs = dt_ioremap ( dt, offset, DWMAC_REG_IDX, DWMAC_REG_LEN );
	if ( ! dwmac->regs ) {
		rc = -ENODEV;
		goto err_ioremap;
	}
	version = readl ( dwmac->regs + DWMAC_VER );
	DBGC ( dwmac, "DWMAC %s version %x.%x (user %x.%x)\n", dwmac->name,
	       DWMAC_VER_CORE_MAJOR ( version ),
	       DWMAC_VER_CORE_MINOR ( version ),
	       DWMAC_VER_USER_MAJOR ( version ),
	       DWMAC_VER_USER_MINOR ( version ) );

	/* Fetch devicetree MAC address */
	if ( ( rc = fdt_mac ( &sysfdt, offset, netdev ) ) != 0 ) {
		DBGC ( dwmac, "DWMAC %s could not fetch MAC: %s\n",
		       dwmac->name, strerror ( rc ) );
		goto err_mac;
	}

	/* Fetch current MAC address, if set */
	mac.reg.addrl = readl ( dwmac->regs + DWMAC_ADDRL );
	mac.reg.addrh = readl ( dwmac->regs + DWMAC_ADDRH );
	memcpy ( netdev->ll_addr, mac.raw, ETH_ALEN );

	/* Reset the NIC */
	if ( ( rc = dwmac_reset ( dwmac ) ) != 0 )
		goto err_reset;

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	/* Update link state */
	dwmac_check_link ( netdev );

	return 0;

	unregister_netdev ( netdev );
 err_register_netdev:
	dwmac_reset ( dwmac );
 err_reset:
 err_mac:
	iounmap ( dwmac->regs );
 err_ioremap:
	netdev_nullify ( netdev );
	netdev_put ( netdev );
 err_alloc:
	return rc;
}

/**
 * Remove devicetree device
 *
 * @v dt		Devicetree device
 */
static void dwmac_remove ( struct dt_device *dt ) {
	struct net_device *netdev = dt_get_drvdata ( dt );
	struct dwmac *dwmac = netdev->priv;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Reset card */
	dwmac_reset ( dwmac );

	/* Free network device */
	iounmap ( dwmac->regs );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** DesignWare MAC compatible model identifiers */
static const char * dwmac_ids[] = {
	"thead,light-dwmac",
	"snps,dwmac",
};

/** DesignWare MAC devicetree driver */
struct dt_driver dwmac_driver __dt_driver = {
	.name = "dwmac",
	.ids = dwmac_ids,
	.id_count = ( sizeof ( dwmac_ids ) / sizeof ( dwmac_ids[0] ) ),
	.probe = dwmac_probe,
	.remove = dwmac_remove,
};
