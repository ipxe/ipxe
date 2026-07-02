/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
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
FILE_SECBOOT ( PERMITTED );

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
#include "virtio-net.h"

/** @file
 *
 * Virtual I/O network device
 *
 */

/** Supported features */
const struct virtio_features virtio_net_features = {
	.word = {
		( VIRTIO_FEAT0_ANY_LAYOUT |
		  VIRTIO_FEAT0_NET_MTU |
		  VIRTIO_FEAT0_NET_MAC ),
		( VIRTIO_FEAT1_MODERN ),
	},
};

/******************************************************************************
 *
 * Device-specific registers
 *
 ******************************************************************************
 */

/**
 * Get MAC address
 *
 * @v netdev		Network device
 */
static void virtio_net_mac ( struct net_device *netdev ) {
	struct virtio_net *vnet = netdev->priv;
	struct virtio_device *virtio = &vnet->virtio;
	uint32_t has_mac;
	unsigned int i;

	/* Read MAC address from device registers */
	for ( i = 0 ; i < ETH_ALEN ; i++ ) {
		netdev->hw_addr[i] = ioread8 ( virtio->device +
					       VIRTIO_NET_MAC + i );
	}

	/* Use random MAC address if undefined or invalid */
	has_mac = ( virtio->features.word[0] & VIRTIO_FEAT0_NET_MAC );
	if ( ! ( has_mac && is_valid_ether_addr ( netdev->hw_addr ) ) ) {
		DBGC ( vnet, "VNET %s has %s MAC address\n",
		       virtio->name, ( has_mac ? "invalid" : "no" ) );
		eth_random_addr ( netdev->hw_addr );
	}
}

/**
 * Get MTU
 *
 * @v netdev		Network device
 */
static void virtio_net_mtu ( struct net_device *netdev ) {
	struct virtio_net *vnet = netdev->priv;
	struct virtio_device *virtio = &vnet->virtio;
	uint32_t has_mtu;

	/* Read MTU from device registers, if available */
	has_mtu = ( virtio->features.word[0] & VIRTIO_FEAT0_NET_MTU );
	if ( has_mtu ) {
		netdev->mtu = ioread16 ( virtio->device + VIRTIO_NET_MTU );
		netdev->max_pkt_len = ( netdev->mtu + ETH_HLEN );
		DBGC ( vnet, "VNET %s has MTU %zd\n",
		       virtio->name, netdev->mtu );
	}
}

/******************************************************************************
 *
 * Queue management
 *
 ******************************************************************************
 */

/**
 * Enable queue
 *
 * @v vnet		Virtio network device
 * @v queue		Virtio network queue
 * @ret rc		Return status code
 */
static int virtio_net_enable ( struct virtio_net *vnet,
			       struct virtio_net_queue *queue ) {
	struct virtio_device *virtio = &vnet->virtio;
	struct virtio_desc *desc;
	unsigned int count;
	unsigned int max;
	unsigned int fill;
	unsigned int slot;
	unsigned int index;
	unsigned int write;
	int rc;

	/* Map packet header */
	if ( ( rc = dma_map ( virtio->dma, &queue->map, &queue->hdr,
			      sizeof ( queue->hdr ), queue->dma ) ) != 0 ) {
		DBGC ( vnet, "VNET %s Q%d could not map header: %s\n",
		       virtio->name, queue->queue.index, strerror ( rc ) );
		goto err_map;
	}

	/* Enable queue */
	count = ( queue->count * VIRTIO_NET_DESCS );
	if ( ( rc = virtio_enable ( virtio, &queue->queue, count ) ) != 0 ) {
		DBGC ( vnet, "VNET %s Q%d could not initialise: %s\n",
		       virtio->name, queue->queue.index, strerror ( rc ) );
		goto err_enable;
	}

	/* Calculate mask */
	max = ( queue->queue.count / VIRTIO_NET_DESCS );
	fill = queue->max;
	if ( fill > max )
		fill = max;
	queue->fill = fill;
	queue->mask = ( fill - 1 );

	/* Initialise descriptors and slot ring */
	write = queue->write;
	for ( slot = 0 ; slot < fill ; slot++ ) {
		queue->slots[slot] = slot;
		queue->iobufs[slot] = NULL;
		index = ( slot * VIRTIO_NET_DESCS );
		desc = &queue->queue.desc[index];
		desc[0].addr = cpu_to_le64 ( dma ( &queue->map, &queue->hdr ));
		desc[0].len = cpu_to_le32 ( vnet->hlen );
		desc[0].flags = cpu_to_le16 ( VIRTIO_DESC_FL_NEXT | write );
		desc[0].next = cpu_to_le16 ( index + 1 );
		desc[1].flags = cpu_to_le16 ( write );
	}

	DBGC ( vnet, "VNET %s Q%d using %d/%d descriptor pairs\n",
	       virtio->name, queue->queue.index, queue->fill, max );
	return 0;

	/* There may be no way to disable individual queues: the
	 * caller must reset the whole device to recover from a
	 * failure.
	 */
 err_enable:
	dma_unmap ( &queue->map, sizeof ( queue->hdr ) );
 err_map:
	return rc;
}

/**
 * Submit I/O buffer to queue
 *
 * @v vnet		Virtio network device
 * @v queue		Virtio network queue
 * @v iobuf		I/O buffer
 * @v len		Submitted length
 */
static void virtio_net_submit ( struct virtio_net *vnet,
				struct virtio_net_queue *queue,
				struct io_buffer *iobuf, size_t len ) {
	struct virtio_device *virtio = &vnet->virtio;
	struct virtio_desc *desc;
	unsigned int prod;
	unsigned int slot;
	unsigned int index;

	/* Get next descriptor pair and consume slot */
	prod = queue->queue.prod;
	slot = queue->slots[ prod & queue->mask ];
	index = ( slot * VIRTIO_NET_DESCS );
	desc = &queue->queue.desc[index];

	/* Populate descriptors */
	desc[1].addr = cpu_to_le64 ( iob_dma ( iobuf ) );
	desc[1].len = cpu_to_le32 ( len );
	DBGC2 ( vnet, "VNET %s Q%d [%02x-%02x] is [%lx,%lx)\n",
		virtio->name, queue->queue.index, index, ( index + 1 ),
		virt_to_phys ( iobuf->data ),
		( virt_to_phys ( iobuf->data ) + len ) );

	/* Record I/O buffer */
	assert ( queue->iobufs[slot] == NULL );
	queue->iobufs[slot] = iobuf;

	/* Submit descriptors */
	virtio_submit ( &queue->queue, index );
}

/**
 * Complete I/O buffer
 *
 * @v vnet		Virtio network device
 * @v queue		Virtio network queue
 * @v len		Length to fill in (or NULL to ignore)
 * @ret iobuf		I/O buffer
 */
static struct io_buffer * virtio_net_complete ( struct virtio_net *vnet,
						struct virtio_net_queue *queue,
						size_t *len ) {
	struct virtio_device *virtio = &vnet->virtio;
	struct io_buffer *iobuf;
	unsigned int cons;
	unsigned int slot;
	unsigned int index;

	/* Complete descriptor pair and recycle slot */
	cons = queue->queue.cons;
	index = virtio_complete ( &queue->queue, len );
	slot = ( index / VIRTIO_NET_DESCS );
	queue->slots[ cons & queue->mask ] = slot;

	/* Complete I/O buffer */
	iobuf = queue->iobufs[slot];
	assert ( iobuf != NULL );
	queue->iobufs[slot] = NULL;
	DBGC2 ( vnet, "VNET %s Q%d [%02x-%02x] complete",
		virtio->name, queue->queue.index, index, ( index + 1 ) );
	if ( len )
		DBGC2 ( vnet, " len %#zx\n", *len );
	DBGC2 ( vnet, "\n" );

	return iobuf;
}

/******************************************************************************
 *
 * Network device interface
 *
 ******************************************************************************
 */

/**
 * Refill receive queue
 *
 * @v vnet		Virtio network device
 */
static void virtio_net_refill_rx ( struct virtio_net *vnet ) {
	struct virtio_device *virtio = &vnet->virtio;
	struct virtio_net_queue *queue = &vnet->rx;
	struct io_buffer *iobuf;
	size_t len = vnet->mfs;
	unsigned int refilled = 0;

	/* Refill queue */
	while ( ( queue->queue.prod - queue->queue.cons ) < queue->fill ) {

		/* Allocate I/O buffer */
		iobuf = alloc_rx_iob ( len, virtio->dma );
		if ( ! iobuf ) {
			/* Wait for next refill */
			break;
		}

		/* Submit I/O buffer */
		virtio_net_submit ( vnet, queue, iobuf, len );
		refilled++;
	}

	/* Notify queue, if applicable */
	if ( refilled )
		virtio_notify ( &queue->queue );
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int virtio_net_open ( struct net_device *netdev ) {
	struct virtio_net *vnet = netdev->priv;
	struct virtio_device *virtio = &vnet->virtio;
	union virtio_net_header hdr;
	int rc;

	/* (Re)initialise device */
	if ( ( rc = virtio_init ( virtio, &virtio_net_features ) ) != 0 ) {
		DBGC ( vnet, "VNET %s could not initialise: %s\n",
		       virtio->name, strerror ( rc ) );
		goto err_init;
	}

	/* Calculate header length */
	vnet->hlen = ( virtio_is_legacy ( virtio ) ?
		       sizeof ( hdr.legacy ) : sizeof ( hdr.modern ) );

	/* Calculate maximum frame size */
	vnet->mfs = ( ETH_HLEN + 4 /* possible VLAN */ + netdev->mtu );

	/* Enable receive queue */
	if ( ( rc = virtio_net_enable ( vnet, &vnet->rx ) ) != 0 ) {
		DBGC ( vnet, "VNET %s could not enable RX: %s\n",
		       virtio->name, strerror ( rc ) );
		goto err_rx;
	}

	/* Enable transmit queue */
	if ( ( rc = virtio_net_enable ( vnet, &vnet->tx ) ) != 0 ) {
		DBGC ( vnet, "VNET %s could not enable TX: %s\n",
		       virtio->name, strerror ( rc ) );
		goto err_tx;
	}

	/* Report driver readiness */
	virtio_status ( virtio, VIRTIO_STAT_DRIVER_OK );

	/* Refill receive queue */
	virtio_net_refill_rx ( vnet );

	return 0;

	dma_unmap ( &vnet->tx.map, sizeof ( vnet->tx.hdr ) );
 err_tx:
	dma_unmap ( &vnet->rx.map, sizeof ( vnet->rx.hdr ) );
 err_rx:
	/* There may be no way to disable individual queues: we must
	 * reset the whole device instead and then free the queues.
	 */
	virtio_reset ( virtio );
	virtio_free ( virtio, &vnet->rx.queue );
	virtio_free ( virtio, &vnet->tx.queue );
 err_init:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void virtio_net_close ( struct net_device *netdev ) {
	struct virtio_net *vnet = netdev->priv;
	struct virtio_device *virtio = &vnet->virtio;
	unsigned int i;

	/* Reset device */
	virtio_reset ( virtio );

	/* Unmap headers (now that device is guaranteed idle) */
	dma_unmap ( &vnet->rx.map, sizeof ( vnet->rx.hdr ) );
	dma_unmap ( &vnet->tx.map, sizeof ( vnet->tx.hdr ) );

	/* Free queues */
	virtio_free ( virtio, &vnet->rx.queue );
	virtio_free ( virtio, &vnet->tx.queue );

	/* Discard any incomplete RX buffers */
	for ( i = 0 ; i < VIRTIO_NET_RX_MAX ; i++ )
		free_rx_iob ( vnet->rx_iobufs[i] );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int virtio_net_transmit ( struct net_device *netdev,
				 struct io_buffer *iobuf ) {
	struct virtio_net *vnet = netdev->priv;
	struct virtio_device *virtio = &vnet->virtio;
	struct virtio_net_queue *queue = &vnet->tx;

	/* Check for an available transmit descriptor */
	if ( ( queue->queue.prod - queue->queue.cons ) >= queue->fill ) {
		DBGC ( vnet, "VNET %s out of transmit descriptors\n",
		       virtio->name );
		return -ENOBUFS;
	}

	/* Submit I/O buffer */
	virtio_net_submit ( vnet, queue, iobuf, iob_len ( iobuf ) );

	/* Notify queue */
	virtio_notify ( &queue->queue );

	return 0;
}

/**
 * Poll for completed packets
 *
 * @v netdev		Network device
 */
static void virtio_net_poll_tx ( struct net_device *netdev ) {
	struct virtio_net *vnet = netdev->priv;
	struct virtio_net_queue *queue = &vnet->tx;
	struct io_buffer *iobuf;

	/* Poll for completed descriptors */
	while ( virtio_completions ( &queue->queue ) ) {

		/* Complete I/O buffer */
		iobuf = virtio_net_complete ( vnet, queue, NULL );
		netdev_tx_complete ( netdev, iobuf );
	}
}

/**
 * Poll for received packets
 *
 * @v netdev		Network device
 */
static void virtio_net_poll_rx ( struct net_device *netdev ) {
	struct virtio_net *vnet = netdev->priv;
	struct virtio_net_queue *queue = &vnet->rx;
	struct io_buffer *iobuf;
	size_t len;

	/* Poll for completed descriptors */
	while ( virtio_completions ( &queue->queue ) > 0 ) {

		/* Complete I/O buffer */
		iobuf = virtio_net_complete ( vnet, queue, &len );
		iob_put ( iobuf, ( len - vnet->hlen ) );
		netdev_rx ( netdev, iobuf );
	}
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void virtio_net_poll ( struct net_device *netdev ) {
	struct virtio_net *vnet = netdev->priv;

	/* Poll for completed packets */
	virtio_net_poll_tx ( netdev );

	/* Poll for received packets */
	virtio_net_poll_rx ( netdev );

	/* Refill receive queue */
	virtio_net_refill_rx ( vnet );
}

/** Virtio network device operations */
static struct net_device_operations virtio_net_operations = {
	.open		= virtio_net_open,
	.close		= virtio_net_close,
	.transmit	= virtio_net_transmit,
	.poll		= virtio_net_poll,
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
static int virtio_net_probe ( struct pci_device *pci ) {
	struct net_device *netdev;
	struct virtio_net *vnet;
	struct virtio_device *virtio;
	int rc;

	/* Allocate and initialise net device */
	netdev = alloc_etherdev ( sizeof ( *vnet ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &virtio_net_operations );
	vnet = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	netdev->dev = &pci->dev;
	netdev->dma = &pci->dma;
	memset ( vnet, 0, sizeof ( *vnet ) );
	virtio = &vnet->virtio;
	virtio_net_queue_init ( &vnet->rx, vnet->rx_iobufs, vnet->rx_slots,
				VIRTIO_NET_RX_INDEX, VIRTIO_NET_RX_COUNT,
				VIRTIO_NET_RX_MAX, DMA_RX,
				VIRTIO_DESC_FL_WRITE );
	virtio_net_queue_init ( &vnet->tx, vnet->tx_iobufs, vnet->tx_slots,
				VIRTIO_NET_TX_INDEX, VIRTIO_NET_TX_COUNT,
				VIRTIO_NET_TX_MAX, DMA_TX, 0 );

	/* Map PCI device */
	if ( ( rc = virtio_pci_map ( virtio, pci ) ) != 0 ) {
		DBGC ( vnet, "VNET %s could not map: %s\n",
		       virtio->name, strerror ( rc ) );
		goto err_pci_map;
	}

	/* Initialise device */
	if ( ( rc = virtio_init ( virtio, &virtio_net_features ) ) != 0 ) {
		DBGC ( vnet, "VNET %s could not initialise: %s\n",
		       virtio->name, strerror ( rc ) );
		goto err_init;
	}

	/* Get MAC address */
	virtio_net_mac ( netdev );

	/* Set MTU */
	virtio_net_mtu ( netdev );

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register;

	/* Mark as link up, since we have no way to test link state changes */
	netdev_link_up ( netdev );

	return 0;

	unregister_netdev ( netdev );
 err_register:
	virtio_reset ( virtio );
 err_init:
	virtio_unmap ( virtio );
 err_pci_map:
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
static void virtio_net_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct virtio_net *vnet = netdev->priv;
	struct virtio_device *virtio = &vnet->virtio;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Reset device */
	virtio_reset ( virtio );

	/* Free network device */
	virtio_unmap ( virtio );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** Virtio network PCI device IDs */
static struct pci_device_id virtio_net_ids[] = {
	PCI_ROM ( 0x1af4, 0x1000, "virtio-net", "Virtio (legacy)", 0 ),
	PCI_ROM ( 0x1af4, 0x1041, "virtio-net", "Virtio (modern)", 0 ),
};

/** Virtio network PCI driver */
struct pci_driver virtio_net_driver __pci_driver = {
	.ids = virtio_net_ids,
	.id_count = ( sizeof ( virtio_net_ids ) /
		      sizeof ( virtio_net_ids[0] ) ),
	.probe = virtio_net_probe,
	.remove = virtio_net_remove,
};
