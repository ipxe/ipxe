/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <ipxe/malloc.h>
#include <ipxe/pci.h>
#include "rdc.h"

/** @file
 *
 * RDC R6040 network driver
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
 * @v rdc		RDC device
 * @ret rc		Return status code
 */
static int rdc_reset ( struct rdc_nic *rdc ) {
	unsigned int i;

	/* Reset NIC */
	writew ( RDC_MCR1_RST, rdc->regs + RDC_MCR1 );

	/* Wait for reset to complete */
	for ( i = 0 ; i < RDC_RESET_MAX_WAIT_MS ; i++ ) {

		/* Check for reset completion */
		if ( readw ( rdc->regs + RDC_MCR1 ) & RDC_MCR1_RST ) {
			mdelay ( 1 );
			continue;
		}

		/* Reset internal state machine */
		writew ( RDC_MACSM_RST, rdc->regs + RDC_MACSM );
		writew ( 0, rdc->regs + RDC_MACSM );
		mdelay ( RDC_MACSM_RESET_DELAY_MS );

		return 0;
	}

	DBGC ( rdc, "RDC %p timed out waiting for reset\n", rdc );
	return -ETIMEDOUT;
}

/******************************************************************************
 *
 * MII interface
 *
 ******************************************************************************
 */

/**
 * Read from MII register
 *
 * @v mdio		MII interface
 * @v phy		PHY address
 * @v reg		Register address
 * @ret value		Data read, or negative error
 */
static int rdc_mii_read ( struct mii_interface *mdio, unsigned int phy,
			  unsigned int reg ) {
	struct rdc_nic *rdc = container_of ( mdio, struct rdc_nic, mdio );
	uint16_t mmdio;
	unsigned int i;

	/* Initiate read */
	mmdio = ( RDC_MMDIO_MIIRD | RDC_MMDIO_PHYAD ( phy ) |
		  RDC_MMDIO_REGAD ( reg ) );
	writew ( mmdio, rdc->regs + RDC_MMDIO );

	/* Wait for read to complete */
	for ( i = 0 ; i < RDC_MII_MAX_WAIT_US ; i++ ) {

		/* Check for read completion */
		if ( readw ( rdc->regs + RDC_MMDIO ) & RDC_MMDIO_MIIRD ) {
			udelay ( 1 );
			continue;
		}

		/* Return register value */
		return ( readw ( rdc->regs + RDC_MMRD ) );
	}

	DBGC ( rdc, "RDC %p timed out waiting for MII read\n", rdc );
	return -ETIMEDOUT;
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
static int rdc_mii_write ( struct mii_interface *mdio, unsigned int phy,
			   unsigned int reg, unsigned int data ) {
	struct rdc_nic *rdc = container_of ( mdio, struct rdc_nic, mdio );
	uint16_t mmdio;
	unsigned int i;

	/* Initiate write */
	mmdio = ( RDC_MMDIO_MIIWR | RDC_MMDIO_PHYAD ( phy ) |
		  RDC_MMDIO_REGAD ( reg ) );
	writew ( data, rdc->regs + RDC_MMWD );
	writew ( mmdio, rdc->regs + RDC_MMDIO );

	/* Wait for write to complete */
	for ( i = 0 ; i < RDC_MII_MAX_WAIT_US ; i++ ) {

		/* Check for write completion */
		if ( readw ( rdc->regs + RDC_MMDIO ) & RDC_MMDIO_MIIWR ) {
			udelay ( 1 );
			continue;
		}

		return 0;
	}

	DBGC ( rdc, "RDC %p timed out waiting for MII write\n", rdc );
	return -ETIMEDOUT;
}

/** RDC MII operations */
static struct mii_operations rdc_mii_operations = {
	.read = rdc_mii_read,
	.write = rdc_mii_write,
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
 * @v rdc		RDC device
 * @ret rc		Return status code
 */
static int rdc_init_phy ( struct rdc_nic *rdc ) {
	int rc;

	/* Find PHY address */
	if ( ( rc = mii_find ( &rdc->mii ) ) != 0 ) {
		DBGC ( rdc, "RDC %p could not find PHY address: %s\n",
		       rdc, strerror ( rc ) );
		return rc;
	}

	/* Reset PHY */
	if ( ( rc = mii_reset ( &rdc->mii ) ) != 0 ) {
		DBGC ( rdc, "RDC %p could not reset PHY: %s\n",
		       rdc, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Check link state
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int rdc_check_link ( struct net_device *netdev ) {
	struct rdc_nic *rdc = netdev->priv;
	int rc;

	/* Check link state */
	if ( ( rc = mii_check_link ( &rdc->mii, netdev ) ) != 0 ) {
		DBGC ( rdc, "RDC %p could not check link: %s\n",
		       rdc, strerror ( rc ) );
		return rc;
	}

	return 0;
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
 * @v rdc		RDC device
 * @v ring		Descriptor ring
 * @ret rc		Return status code
 */
static int rdc_create_ring ( struct rdc_nic *rdc, struct rdc_ring *ring ) {
	size_t len = ( ring->count * sizeof ( ring->desc[0] ) );
	struct rdc_descriptor *desc;
	struct rdc_descriptor *next;
	physaddr_t start;
	unsigned int i;

	/* Allocate descriptor ring */
	ring->desc = dma_alloc ( rdc->dma, &ring->map, len, len );
	if ( ! ring->desc )
		return -ENOMEM;

	/* Initialise descriptor ring */
	memset ( ring->desc, 0, len );
	for ( i = 0 ; i < ring->count ; i++ ) {
		desc = &ring->desc[i];
		next = &ring->desc[ ( i + 1 ) & ( ring->count - 1 ) ];
		desc->next = cpu_to_le32 ( dma ( &ring->map, next ) );
	}

	/* Program ring address */
	start = dma ( &ring->map, ring->desc );
	writew ( ( start >> 0 ), ( rdc->regs + ring->reg + RDC_MxDSA_LO ) );
	writew ( ( start >> 16 ), ( rdc->regs + ring->reg + RDC_MxDSA_HI ) );

	DBGC ( rdc, "RDC %p ring %#02x is at [%08lx,%08lx)\n",
	       rdc, ring->reg, virt_to_phys ( ring->desc ),
	       ( virt_to_phys ( ring->desc ) + len ) );
	return 0;
}

/**
 * Destroy descriptor ring
 *
 * @v rdc		RDC device
 * @v ring		Descriptor ring
 */
static void rdc_destroy_ring ( struct rdc_nic *rdc, struct rdc_ring *ring ) {
	size_t len = ( ring->count * sizeof ( ring->desc[0] ) );

	/* Clear ring address */
	writew ( 0, ( rdc->regs + ring->reg + RDC_MxDSA_LO ) );
	writew ( 0, ( rdc->regs + ring->reg + RDC_MxDSA_HI ) );

	/* Free descriptors */
	dma_free ( &ring->map, ring->desc, len );
	ring->desc = NULL;

	/* Reset ring */
	ring->prod = 0;
	ring->cons = 0;
}

/**
 * Refill receive descriptor ring
 *
 * @v rdc		RDC device
 */
static void rdc_refill_rx ( struct rdc_nic *rdc ) {
	struct rdc_descriptor *rx;
	struct io_buffer *iobuf;
	unsigned int rx_idx;

	/* Refill ring */
	while ( ( rdc->rx.prod - rdc->rx.cons ) < RDC_NUM_RX_DESC ) {

		/* Allocate I/O buffer */
		iobuf = alloc_rx_iob ( RDC_RX_MAX_LEN, rdc->dma );
		if ( ! iobuf ) {
			/* Wait for next refill */
			break;
		}

		/* Get next receive descriptor */
		rx_idx = ( rdc->rx.prod++ % RDC_NUM_RX_DESC );
		rx = &rdc->rx.desc[rx_idx];

		/* Populate receive descriptor */
		rx->len = cpu_to_le16 ( RDC_RX_MAX_LEN );
		rx->addr = cpu_to_le32 ( iob_dma ( iobuf ) );
		wmb();
		rx->flags = cpu_to_le16 ( RDC_FL_OWNED );

		/* Record I/O buffer */
		assert ( rdc->rx_iobuf[rx_idx] == NULL );
		rdc->rx_iobuf[rx_idx] = iobuf;

		DBGC2 ( rdc, "RDC %p RX %d is [%lx,%lx)\n",
			rdc, rx_idx, virt_to_phys ( iobuf->data ),
			( virt_to_phys ( iobuf->data ) + RDC_RX_MAX_LEN ) );
	}
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int rdc_open ( struct net_device *netdev ) {
	struct rdc_nic *rdc = netdev->priv;
	int rc;

	/* Create transmit descriptor ring */
	if ( ( rc = rdc_create_ring ( rdc, &rdc->tx ) ) != 0 )
		goto err_create_tx;

	/* Create receive descriptor ring */
	if ( ( rc = rdc_create_ring ( rdc, &rdc->rx ) ) != 0 )
		goto err_create_rx;

	/* Program receive buffer length */
	writew ( RDC_RX_MAX_LEN, rdc->regs + RDC_MRBSR );

	/* Enable transmit and receive */
	writew ( ( RDC_MCR0_FD | RDC_MCR0_TXEN | RDC_MCR0_PROMISC |
		   RDC_MCR0_RXEN ),
		 rdc->regs + RDC_MCR0 );

	/* Enable PHY status polling */
	writew ( ( RDC_MPSCCR_EN | RDC_MPSCCR_PHYAD ( rdc->mii.address ) |
		   RDC_MPSCCR_SLOW ),
		 rdc->regs + RDC_MPSCCR );

	/* Fill receive ring */
	rdc_refill_rx ( rdc );

	/* Update link state */
	rdc_check_link ( netdev );

	return 0;

	rdc_destroy_ring ( rdc, &rdc->rx );
 err_create_rx:
	rdc_destroy_ring ( rdc, &rdc->tx );
 err_create_tx:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void rdc_close ( struct net_device *netdev ) {
	struct rdc_nic *rdc = netdev->priv;
	unsigned int i;

	/* Disable NIC */
	writew ( 0, rdc->regs + RDC_MCR0 );

	/* Destroy receive descriptor ring */
	rdc_destroy_ring ( rdc, &rdc->rx );

	/* Discard any unused receive buffers */
	for ( i = 0 ; i < RDC_NUM_RX_DESC ; i++ ) {
		if ( rdc->rx_iobuf[i] )
			free_rx_iob ( rdc->rx_iobuf[i] );
		rdc->rx_iobuf[i] = NULL;
	}

	/* Destroy transmit descriptor ring */
	rdc_destroy_ring ( rdc, &rdc->tx );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int rdc_transmit ( struct net_device *netdev, struct io_buffer *iobuf ) {
	struct rdc_nic *rdc = netdev->priv;
	struct rdc_descriptor *tx;
	unsigned int tx_idx;
	int rc;

	/* Get next transmit descriptor */
	if ( ( rdc->tx.prod - rdc->tx.cons ) >= RDC_NUM_TX_DESC ) {
		DBGC ( rdc, "RDC %p out of transmit descriptors\n", rdc );
		return -ENOBUFS;
	}
	tx_idx = ( rdc->tx.prod % RDC_NUM_TX_DESC );
	tx = &rdc->tx.desc[tx_idx];

	/* Pad to minimum length */
	iob_pad ( iobuf, ETH_ZLEN );

	/* Map I/O buffer */
	if ( ( rc = iob_map_tx ( iobuf, rdc->dma ) ) != 0 )
		return rc;

	/* Update producer index */
	rdc->tx.prod++;

	/* Populate transmit descriptor */
	tx->len = cpu_to_le16 ( iob_len ( iobuf ) );
	tx->addr = cpu_to_le32 ( iob_dma ( iobuf ) );
	wmb();
	tx->flags = cpu_to_le16 ( RDC_FL_OWNED );
	wmb();

	/* Notify card that there are packets ready to transmit */
	writew ( RDC_MTPR_TM2TX, rdc->regs + RDC_MTPR );

	return 0;
}

/**
 * Poll for completed packets
 *
 * @v netdev		Network device
 */
static void rdc_poll_tx ( struct net_device *netdev ) {
	struct rdc_nic *rdc = netdev->priv;
	struct rdc_descriptor *tx;
	unsigned int tx_idx;

	/* Check for completed packets */
	while ( rdc->tx.cons != rdc->tx.prod ) {

		/* Get next transmit descriptor */
		tx_idx = ( rdc->tx.cons % RDC_NUM_TX_DESC );
		tx = &rdc->tx.desc[tx_idx];

		/* Stop if descriptor is still in use */
		if ( tx->flags & cpu_to_le16 ( RDC_FL_OWNED ) )
			return;
		DBGC2 ( rdc, "RDC %p TX %d complete\n", rdc, tx_idx );

		/* Complete transmit descriptor */
		rdc->tx.cons++;
		netdev_tx_complete_next ( netdev );
	}
}

/**
 * Poll for received packets
 *
 * @v netdev		Network device
 */
static void rdc_poll_rx ( struct net_device *netdev ) {
	struct rdc_nic *rdc = netdev->priv;
	struct rdc_descriptor *rx;
	struct io_buffer *iobuf;
	unsigned int rx_idx;
	size_t len;

	/* Check for received packets */
	while ( rdc->rx.cons != rdc->rx.prod ) {

		/* Get next receive descriptor */
		rx_idx = ( rdc->rx.cons % RDC_NUM_RX_DESC );
		rx = &rdc->rx.desc[rx_idx];

		/* Stop if descriptor is still in use */
		if ( rx->flags & cpu_to_le16 ( RDC_FL_OWNED ) )
			return;

		/* Populate I/O buffer */
		iobuf = rdc->rx_iobuf[rx_idx];
		rdc->rx_iobuf[rx_idx] = NULL;
		len = le16_to_cpu ( rx->len );
		iob_put ( iobuf, len );
		iob_unput ( iobuf, 4 /* strip CRC */ );

		/* Hand off to network stack */
		if ( rx->flags & cpu_to_le16 ( RDC_FL_OK ) ) {
			DBGC2 ( rdc, "RDC %p RX %d complete (length %zd)\n",
				rdc, rx_idx, len );
			netdev_rx ( netdev, iobuf );
		} else {
			DBGC2 ( rdc, "RDC %p RX %d error (length %zd, "
				"flags %#04x)\n", rdc, rx_idx, len,
				le16_to_cpu ( rx->flags ) );
			netdev_rx_err ( netdev, iobuf, -EIO );
		}
		rdc->rx.cons++;
	}
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void rdc_poll ( struct net_device *netdev ) {
	struct rdc_nic *rdc = netdev->priv;
	uint16_t misr;

	/* Check for (and acknowledge) interrupts */
	misr = readw ( rdc->regs + RDC_MISR );

	/* Poll for TX completions, if applicable */
	if ( misr & RDC_MIRQ_TX )
		rdc_poll_tx ( netdev );

	/* Poll for RX completions, if applicable */
	if ( misr & RDC_MIRQ_RX )
		rdc_poll_rx ( netdev );

	/* Check link state, if applicable */
	if ( misr & RDC_MIRQ_LINK )
		rdc_check_link ( netdev );

	/* Check for unexpected interrupts */
	if ( misr & ~( RDC_MIRQ_LINK | RDC_MIRQ_TX | RDC_MIRQ_RX_EARLY |
		       RDC_MIRQ_RX_EMPTY | RDC_MIRQ_RX ) ) {
		DBGC ( rdc, "RDC %p unexpected MISR %#04x\n", rdc, misr );
		/* Report as a TX error */
		netdev_tx_err ( netdev, NULL, -ENOTSUP );
	}

	/* Refill receive ring */
	rdc_refill_rx ( rdc );
}

/**
 * Enable or disable interrupts
 *
 * @v netdev		Network device
 * @v enable		Interrupts should be enabled
 */
static void rdc_irq ( struct net_device *netdev, int enable ) {
	struct rdc_nic *rdc = netdev->priv;
	uint16_t mier;

	/* Enable/disable interrupts */
	mier = ( enable ? ( RDC_MIRQ_LINK | RDC_MIRQ_TX | RDC_MIRQ_RX ) : 0 );
	writew ( mier, rdc->regs + RDC_MIER );
}

/** RDC network device operations */
static struct net_device_operations rdc_operations = {
	.open		= rdc_open,
	.close		= rdc_close,
	.transmit	= rdc_transmit,
	.poll		= rdc_poll,
	.irq		= rdc_irq,
};

/******************************************************************************
 *
 * PCI interface
 *
 ******************************************************************************
 */

/**
 * Probe PCI device
 *
 * @v pci		PCI device
 * @ret rc		Return status code
 */
static int rdc_probe ( struct pci_device *pci ) {
	struct net_device *netdev;
	struct rdc_nic *rdc;
	union rdc_mac mac;
	int rc;

	/* Allocate and initialise net device */
	netdev = alloc_etherdev ( sizeof ( *rdc ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &rdc_operations );
	rdc = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	netdev->dev = &pci->dev;
	memset ( rdc, 0, sizeof ( *rdc ) );
	rdc->dma = &pci->dma;
	mdio_init ( &rdc->mdio, &rdc_mii_operations );
	mii_init ( &rdc->mii, &rdc->mdio, 0 );
	rdc_init_ring ( &rdc->tx, RDC_NUM_TX_DESC, RDC_MTDSA );
	rdc_init_ring ( &rdc->rx, RDC_NUM_RX_DESC, RDC_MRDSA );

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Map registers */
	rdc->regs = pci_ioremap ( pci, pci->membase, RDC_BAR_SIZE );
	if ( ! rdc->regs ) {
		rc = -ENODEV;
		goto err_ioremap;
	}

	/* Fetch MAC address */
	mac.mid[0] = cpu_to_le16 ( readw ( rdc->regs + RDC_MID0 ) );
	mac.mid[1] = cpu_to_le16 ( readw ( rdc->regs + RDC_MID1 ) );
	mac.mid[2] = cpu_to_le16 ( readw ( rdc->regs + RDC_MID2 ) );
	memcpy ( netdev->hw_addr, mac.raw, ETH_ALEN );

	/* Reset the NIC */
	if ( ( rc = rdc_reset ( rdc ) ) != 0 )
		goto err_reset;

	/* Initialise PHY */
	if ( ( rc = rdc_init_phy ( rdc ) ) != 0 )
		goto err_init_phy;

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	/* Set initial link state */
	rdc_check_link ( netdev );

	return 0;

	unregister_netdev ( netdev );
 err_register_netdev:
 err_init_phy:
	rdc_reset ( rdc );
 err_reset:
	iounmap ( rdc->regs );
 err_ioremap:
	netdev_nullify ( netdev );
	netdev_put ( netdev );
 err_alloc:
	return rc;
}

/**
 * Remove PCI device
 *
 * @v pci		PCI device
 */
static void rdc_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct rdc_nic *rdc = netdev->priv;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Reset card */
	rdc_reset ( rdc );

	/* Free network device */
	iounmap ( rdc->regs );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** RDC PCI device IDs */
static struct pci_device_id rdc_nics[] = {
	PCI_ROM ( 0x17f3, 0x6040, "r6040", "RDC R6040", 0 ),
};

/** RDC PCI driver */
struct pci_driver rdc_driver __pci_driver = {
	.ids = rdc_nics,
	.id_count = ( sizeof ( rdc_nics ) / sizeof ( rdc_nics[0] ) ),
	.probe = rdc_probe,
	.remove = rdc_remove,
};
