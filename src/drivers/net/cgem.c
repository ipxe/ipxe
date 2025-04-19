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
#include <unistd.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/iobuf.h>
#include <ipxe/timer.h>
#include <ipxe/devtree.h>
#include <ipxe/fdt.h>
#include "cgem.h"

/** @file
 *
 * Cadence Gigabit Ethernet MAC (GEM) network driver
 *
 * Based primarily on the Zynq 7000 SoC Technical Reference Manual,
 * available at the time of writing from:
 *
 *     https://docs.amd.com/r/en-US/ug585-zynq-7000-SoC-TRM
 *
 */

/******************************************************************************
 *
 * Device reset
 *
 ******************************************************************************
 */

/**
 * Reset hardware
 *
 * @v cgem		Cadence GEM device
 * @ret rc		Return status code
 */
static int cgem_reset ( struct cgem_nic *cgem ) {

	/* There is no software-driven reset capability in the
	 * hardware.  Instead we have to write the expected reset
	 * values to the various registers.
	 */

	/* Disable all interrupts */
	writel ( CGEM_IDR_ALL, ( cgem->regs + CGEM_IDR ) );

	/* Clear network control register */
	writel ( 0, ( cgem->regs + CGEM_NWCTRL ) );

	/* Clear statistics registers now that TX and RX are stopped */
	writel ( CGEM_NWCTRL_STATCLR, ( cgem->regs + CGEM_NWCTRL ) );

	/* Clear TX queue base address */
	writel ( 0, ( cgem->regs + CGEM_TXQBASE ) );

	/* Clear RX queue base address */
	writel ( 0, ( cgem->regs + CGEM_RXQBASE ) );

	/* Configure DMA */
	writel ( ( CGEM_DMACR_RXBUF ( CGEM_RX_LEN ) | CGEM_DMACR_TXSIZE_MAX |
		   CGEM_DMACR_RXSIZE_MAX | CGEM_DMACR_BLENGTH_MAX ),
		 ( cgem->regs + CGEM_DMACR ) );

	/* Enable MII interface */
	writel ( CGEM_NWCTRL_MDEN, ( cgem->regs + CGEM_NWCTRL ) );

	return 0;
}

/******************************************************************************
 *
 * PHY access
 *
 ******************************************************************************
 */

/**
 * Wait for MII operation to complete
 *
 * @v cgem		Cadence GEM device
 * @ret rc		Return status code
 */
static int cgem_mii_wait ( struct cgem_nic *cgem ) {
	uint32_t nwsr;
	unsigned int i;

	/* Wait for MII interface to become idle */
	for ( i = 0 ; i < CGEM_MII_MAX_WAIT_US ; i++ ) {

		/* Check if MII interface is idle */
		nwsr = readl ( cgem->regs + CGEM_NWSR );
		if ( nwsr & CGEM_NWSR_MII_IDLE )
			return 0;

		/* Delay */
		udelay ( 1 );
	}

	DBGC ( cgem, "CGEM %s timed out waiting for MII\n", cgem->name );
	return -ETIMEDOUT;
}

/**
 * Read from MII register
 *
 * @v mdio		MII interface
 * @v phy		PHY address
 * @v reg		Register address
 * @ret data		Data read, or negative error
 */
static int cgem_mii_read ( struct mii_interface *mdio, unsigned int phy,
			   unsigned int reg ) {
	struct cgem_nic *cgem = container_of ( mdio, struct cgem_nic, mdio );
	unsigned int data;
	int rc;

	/* Initiate read */
	writel ( ( CGEM_PHYMNTNC_CLAUSE22 | CGEM_PHYMNTNC_OP_READ |
		   CGEM_PHYMNTNC_ADDR ( phy ) | CGEM_PHYMNTNC_REG ( reg ) |
		   CGEM_PHYMNTNC_FIXED ),
		 ( cgem->regs + CGEM_PHYMNTNC ) );

	/* Wait for read to complete */
	if ( ( rc = cgem_mii_wait ( cgem ) ) != 0 )
		return rc;

	/* Read data */
	data = ( readl ( cgem->regs + CGEM_PHYMNTNC ) &
		 CGEM_PHYMNTNC_DATA_MASK );

	return data;
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
static int cgem_mii_write ( struct mii_interface *mdio, unsigned int phy,
			    unsigned int reg, unsigned int data ) {
	struct cgem_nic *cgem = container_of ( mdio, struct cgem_nic, mdio );
	int rc;

	/* Initiate write */
	writel ( ( CGEM_PHYMNTNC_CLAUSE22 | CGEM_PHYMNTNC_OP_READ |
		   CGEM_PHYMNTNC_ADDR ( phy ) | CGEM_PHYMNTNC_REG ( reg ) |
		   CGEM_PHYMNTNC_FIXED | data ),
		 ( cgem->regs + CGEM_PHYMNTNC ) );

	/* Wait for write to complete */
	if ( ( rc = cgem_mii_wait ( cgem ) ) != 0 )
		return rc;

	return 0;
}

/** MII operations */
static struct mii_operations cgem_mii_operations = {
	.read = cgem_mii_read,
	.write = cgem_mii_write,
};

/******************************************************************************
 *
 * Link state
 *
 ******************************************************************************
 */

/**
 * Initialise PHY
 *
 * @v cgem		Cadence GEM device
 * @ret rc		Return status code
 */
static int cgem_init_phy ( struct cgem_nic *cgem ) {
	int rc;

	/* Find PHY address */
	if ( ( rc = mii_find ( &cgem->mii ) ) != 0 ) {
		DBGC ( cgem, "CGEM %s could not find PHY address: %s\n",
		       cgem->name, strerror ( rc ) );
		return rc;
	}

	/* Reset PHY */
	if ( ( rc = mii_reset ( &cgem->mii ) ) != 0 ) {
		DBGC ( cgem, "CGEM %s could not reset PHY: %s\n",
		       cgem->name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Check link state
 *
 * @v netdev		Network device
 */
static int cgem_check_link ( struct net_device *netdev ) {
	struct cgem_nic *cgem = netdev->priv;
	int rc;

	/* Check link state */
	if ( ( rc = mii_check_link ( &cgem->mii, netdev ) ) != 0 ) {
		DBGC ( cgem, "CGEM %s could not check link: %s\n",
		       cgem->name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Check link state periodically
 *
 * @v retry		Link state check timer
 * @v over		Failure indicator
 */
static void cgem_expired ( struct retry_timer *timer, int over __unused ) {
	struct cgem_nic *cgem = container_of ( timer, struct cgem_nic, timer );
	struct net_device *netdev = cgem->netdev;

	/* Restart timer */
	start_timer_fixed ( timer, CGEM_LINK_INTERVAL );

	/* Check link state */
	cgem_check_link ( netdev );
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
 * @v cgem		Cadence GEM device
 * @v ring		Descriptor ring
 * @ret rc		Return status code
 */
static int cgem_create_ring ( struct cgem_nic *cgem, struct cgem_ring *ring ) {
	struct cgem_descriptor *desc;
	unsigned int i;

	/* Allocate descriptor ring (on its own size) */
	ring->desc = dma_alloc ( cgem->dma, &ring->map, ring->len, ring->len );
	if ( ! ring->desc )
		return -ENOMEM;

	/* Initialise descriptor ring */
	for ( i = 0 ; i < ring->count ; i++ ) {
		desc = &ring->desc[i];
		desc->addr = cpu_to_le32 ( CGEM_RX_ADDR_OWNED );
		desc->flags = cpu_to_le32 ( CGEM_TX_FL_OWNED );
	}
	desc = &ring->desc[ ring->count - 1 ];
	desc->addr |= cpu_to_le32 ( CGEM_RX_ADDR_WRAP );
	desc->flags |= cpu_to_le32 ( CGEM_TX_FL_WRAP );

	/* Program ring address */
	writel ( dma ( &ring->map, ring->desc ),
		 ( cgem->regs + ring->qbase ) );

	DBGC ( cgem, "CGEM %s ring %02x is at [%08lx,%08lx)\n",
	       cgem->name, ring->qbase, virt_to_phys ( ring->desc ),
	       ( virt_to_phys ( ring->desc ) + ring->len ) );
	return 0;
}

/**
 * Destroy descriptor ring
 *
 * @v cgem		Cadence GEM device
 * @v ring		Descriptor ring
 */
static void cgem_destroy_ring ( struct cgem_nic *cgem,
				struct cgem_ring *ring ) {

	/* Clear ring address */
	writel ( 0, ( cgem->regs + ring->qbase ) );

	/* Free descriptor ring */
	dma_free ( &ring->map, ring->desc, ring->len );
	ring->desc = NULL;
	ring->prod = 0;
	ring->cons = 0;
}

/**
 * Refill receive descriptor ring
 *
 * @v cgem		Cadence GEM device
 */
static void cgem_refill_rx ( struct cgem_nic *cgem ) {
	struct cgem_descriptor *rx;
	struct io_buffer *iobuf;
	unsigned int rx_idx;
	uint32_t addr;

	/* Refill ring */
	while ( ( cgem->rx.prod - cgem->rx.cons ) != CGEM_NUM_RX_DESC ) {

		/* Allocate I/O buffer */
		iobuf = alloc_rx_iob ( CGEM_RX_LEN, cgem->dma );
		if ( ! iobuf ) {
			/* Wait for next refill */
			break;
		}

		/* Get next receive descriptor */
		rx_idx = ( cgem->rx.prod++ % CGEM_NUM_RX_DESC );
		rx = &cgem->rx.desc[rx_idx];

		/* Populate receive descriptor */
		rx->flags = 0;
		addr = 0;
		if ( ( cgem->rx.prod % CGEM_NUM_RX_DESC ) == 0 )
			addr |= CGEM_RX_ADDR_WRAP;
		rx->addr = cpu_to_le32 ( addr | iob_dma ( iobuf ) );

		/* Record I/O buffer */
		assert ( cgem->rx_iobuf[rx_idx] == NULL );
		cgem->rx_iobuf[rx_idx] = iobuf;

		DBGC2 ( cgem, "CGEM %s RX %d is [%08lx,%08lx)\n",
			cgem->name, rx_idx, virt_to_phys ( iobuf->data ),
			( virt_to_phys ( iobuf->data ) + CGEM_RX_LEN ) );
	}
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int cgem_open ( struct net_device *netdev ) {
	struct cgem_nic *cgem = netdev->priv;
	union cgem_mac mac;
	int rc;

	/* Create transmit descriptor ring */
	if ( ( rc = cgem_create_ring ( cgem, &cgem->tx ) ) != 0 )
		goto err_create_tx;

	/* Create receive descriptor ring */
	if ( ( rc = cgem_create_ring ( cgem, &cgem->rx ) ) != 0 )
		goto err_create_rx;

	/* Set MAC address */
	memcpy ( mac.raw, netdev->ll_addr, ETH_ALEN );
	writel ( mac.reg.low, ( cgem->regs + CGEM_LADDRL ) );
	writel ( mac.reg.high, ( cgem->regs + CGEM_LADDRH ) );

	/* Enable transmit and receive */
	writel ( CGEM_NWCTRL_NORMAL, ( cgem->regs + CGEM_NWCTRL ) );

	/* Refill receive descriptor ring */
	cgem_refill_rx ( cgem );

	/* Update link state */
	cgem_check_link ( netdev );

	/* Start link state timer */
	start_timer_fixed ( &cgem->timer, CGEM_LINK_INTERVAL );

	return 0;

	cgem_destroy_ring ( cgem, &cgem->rx );
 err_create_rx:
	cgem_destroy_ring ( cgem, &cgem->tx );
 err_create_tx:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void cgem_close ( struct net_device *netdev ) {
	struct cgem_nic *cgem = netdev->priv;
	unsigned int i;

	/* Stop link state timer */
	stop_timer ( &cgem->timer );

	/* Reset NIC */
	cgem_reset ( cgem );

	/* Discard unused receive buffers */
	for ( i = 0 ; i < CGEM_NUM_RX_DESC ; i++ ) {
		if ( cgem->rx_iobuf[i] )
			free_rx_iob ( cgem->rx_iobuf[i] );
		cgem->rx_iobuf[i] = NULL;
	}

	/* Destroy receive descriptor ring */
	cgem_destroy_ring ( cgem, &cgem->rx );

	/* Destroy transmit descriptor ring */
	cgem_destroy_ring ( cgem, &cgem->tx );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int cgem_transmit ( struct net_device *netdev,
			   struct io_buffer *iobuf ) {
	struct cgem_nic *cgem = netdev->priv;
	struct cgem_descriptor *tx;
	unsigned int tx_idx;
	uint32_t flags;
	int rc;

	/* Get next transmit descriptor */
	if ( ( cgem->tx.prod - cgem->tx.cons ) >= CGEM_NUM_TX_DESC ) {
		DBGC ( cgem, "CGEM %s out of transmit descriptors\n",
		       cgem->name );
		return -ENOBUFS;
	}
	tx_idx = ( cgem->tx.prod % CGEM_NUM_TX_DESC );
	tx = &cgem->tx.desc[tx_idx];

	/* Pad to minimum length */
	iob_pad ( iobuf, ETH_ZLEN );

	/* Map I/O buffer */
	if ( ( rc = iob_map_tx ( iobuf, cgem->dma ) ) != 0 )
		return rc;

	/* Update producer index */
	cgem->tx.prod++;

	/* Populate transmit descriptor */
	flags = CGEM_TX_FL_LAST;
	if ( ( cgem->tx.prod % CGEM_NUM_TX_DESC ) == 0 )
		flags |= CGEM_TX_FL_WRAP;
	tx->addr = cpu_to_le32 ( iob_dma ( iobuf ) );
	wmb();
	tx->flags = cpu_to_le32 ( flags | iob_len ( iobuf ) );
	wmb();

	/* Initiate transmission */
	writel ( ( CGEM_NWCTRL_NORMAL | CGEM_NWCTRL_STARTTX ),
		 ( cgem->regs + CGEM_NWCTRL ) );

	DBGC2 ( cgem, "CGEM %s TX %d is [%08lx,%08lx)\n",
		cgem->name, tx_idx, virt_to_phys ( iobuf->data ),
		( virt_to_phys ( iobuf->data ) + iob_len ( iobuf ) ) );
	return 0;
}

/**
 * Poll for completed packets
 *
 * @V netdev		Network device
 */
static void cgem_poll_tx ( struct net_device *netdev ) {
	struct cgem_nic *cgem = netdev->priv;
	struct cgem_descriptor *tx;
	unsigned int tx_idx;

	/* Check for completed packets */
	while ( cgem->tx.cons != cgem->tx.prod ) {

		/* Get next transmit descriptor */
		tx_idx = ( cgem->tx.cons % CGEM_NUM_TX_DESC );
		tx = &cgem->tx.desc[tx_idx];

		/* Stop if descriptor is still owned by hardware */
		if ( ! ( tx->flags & cpu_to_le32 ( CGEM_TX_FL_OWNED ) ) )
			return;
		DBGC2 ( cgem, "CGEM %s TX %d complete\n",
			cgem->name, tx_idx );

		/* Complete transmit descriptor */
		netdev_tx_complete_next ( netdev );
		cgem->tx.cons++;
	}
}

/**
 * Poll for received packets
 *
 * @v netdev		Network device
 */
static void cgem_poll_rx ( struct net_device *netdev ) {
	struct cgem_nic *cgem = netdev->priv;
	struct cgem_descriptor *rx;
	struct io_buffer *iobuf;
	unsigned int rx_idx;
	uint32_t flags;
	size_t len;

	/* Check for received packets */
	while ( cgem->rx.cons != cgem->rx.prod ) {

		/* Get next receive descriptor */
		rx_idx = ( cgem->rx.cons % CGEM_NUM_RX_DESC );
		rx = &cgem->rx.desc[rx_idx];

		/* Stop if descriptor is still in use */
		if ( ! ( rx->addr & cpu_to_le32 ( CGEM_RX_ADDR_OWNED ) ) )
			return;

		/* Populate I/O buffer */
		iobuf = cgem->rx_iobuf[rx_idx];
		cgem->rx_iobuf[rx_idx] = NULL;
		flags = le32_to_cpu ( rx->flags );
		len = CGEM_RX_FL_LEN ( flags );
		iob_put ( iobuf, len );
		DBGC2 ( cgem, "CGEM %s RX %d complete (length %zd)\n",
			cgem->name, rx_idx, len );

		/* Hand off to network stack */
		netdev_rx ( netdev, iobuf );
		cgem->rx.cons++;
	}
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void cgem_poll ( struct net_device *netdev ) {
	struct cgem_nic *cgem = netdev->priv;

	/* Poll for TX competions */
	cgem_poll_tx ( netdev );

	/* Poll for RX completions */
	cgem_poll_rx ( netdev );

	/* Refill RX ring */
	cgem_refill_rx ( cgem );
}

/** Cadence GEM network device operations */
static struct net_device_operations cgem_operations = {
	.open		= cgem_open,
	.close		= cgem_close,
	.transmit	= cgem_transmit,
	.poll		= cgem_poll,
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
static int cgem_probe ( struct dt_device *dt, unsigned int offset ) {
	struct net_device *netdev;
	struct cgem_nic *cgem;
	union cgem_mac mac;
	int rc;

	/* Allocate and initialise net device */
	netdev = alloc_etherdev ( sizeof ( *cgem ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &cgem_operations );
	cgem = netdev->priv;
	dt_set_drvdata ( dt, netdev );
	netdev->dev = &dt->dev;
	memset ( cgem, 0, sizeof ( *cgem ) );
	cgem->dma = &dt->dma;
	cgem->netdev = netdev;
	cgem->name = netdev->dev->name;
	mdio_init ( &cgem->mdio, &cgem_mii_operations );
	mii_init ( &cgem->mii, &cgem->mdio, 0 );
	timer_init ( &cgem->timer, cgem_expired, &netdev->refcnt );
	cgem_init_ring ( &cgem->tx, CGEM_NUM_TX_DESC, CGEM_TXQBASE );
	cgem_init_ring ( &cgem->rx, CGEM_NUM_RX_DESC, CGEM_RXQBASE );

	/* Map registers */
	cgem->regs = dt_ioremap ( dt, offset, CGEM_REG_IDX, CGEM_REG_LEN );
	if ( ! cgem->regs ) {
		rc = -ENODEV;
		goto err_ioremap;
	}

	/* Reset the NIC */
	if ( ( rc = cgem_reset ( cgem ) ) != 0 )
		goto err_reset;

	/* Initialise the PHY */
	if ( ( rc = cgem_init_phy ( cgem ) ) != 0 )
		goto err_init_phy;

	/* Fetch devicetree MAC address */
	if ( ( rc = fdt_mac ( &sysfdt, offset, netdev ) ) != 0 ) {
		DBGC ( cgem, "CGEM %s could not fetch MAC: %s\n",
		       cgem->name, strerror ( rc ) );
		goto err_mac;
	}

	/* Fetch current MAC address, if set */
	mac.reg.low = readl ( cgem->regs + CGEM_LADDRL );
	mac.reg.high = readl ( cgem->regs + CGEM_LADDRH );
	memcpy ( netdev->ll_addr, mac.raw, ETH_ALEN );

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	/* Set initial link state */
	cgem_check_link ( netdev );

	return 0;

	unregister_netdev ( netdev );
 err_register_netdev:
 err_mac:
 err_init_phy:
	cgem_reset ( cgem );
 err_reset:
	iounmap ( cgem->regs );
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
static void cgem_remove ( struct dt_device *dt ) {
	struct net_device *netdev = dt_get_drvdata ( dt );
	struct cgem_nic *cgem = netdev->priv;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Reset card */
	cgem_reset ( cgem );

	/* Free network device */
	iounmap ( cgem->regs );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** Cadence GEM compatible model identifiers */
static const char * cgem_ids[] = {
	"sifive,fu540-c000-gem",
};

/** Cadence GEM devicetree driver */
struct dt_driver cgem_driver __dt_driver = {
	.name = "cgem",
	.ids = cgem_ids,
	.id_count = ( sizeof ( cgem_ids ) / sizeof ( cgem_ids[0] ) ),
	.probe = cgem_probe,
	.remove = cgem_remove,
};
