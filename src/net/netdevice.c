/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdint.h>
#include <stdlib.h>
#include <byteswap.h>
#include <string.h>
#include <errno.h>
#include <gpxe/if_ether.h>
#include <gpxe/pkbuff.h>
#include <gpxe/tables.h>
#include <gpxe/process.h>
#include <gpxe/init.h>
#include <gpxe/netdevice.h>

/** @file
 *
 * Network device management
 *
 */

/** Registered network-layer protocols */
static struct net_protocol net_protocols[0] __table_start ( net_protocols );
static struct net_protocol net_protocols_end[0] __table_end ( net_protocols );

/** List of network devices */
static LIST_HEAD ( net_devices );

/**
 * Transmit raw packet via network device
 *
 * @v netdev		Network device
 * @v pkb		Packet buffer
 * @ret rc		Return status code
 *
 * Transmits the packet via the specified network device.  This
 * function takes ownership of the packet buffer.
 */
int netdev_tx ( struct net_device *netdev, struct pk_buff *pkb ) {
	int rc;

	DBGC ( netdev, "NETDEV %p transmitting %p (%p+%zx)\n",
	       netdev, pkb, pkb->data, pkb_len ( pkb ) );

	list_add_tail ( &pkb->list, &netdev->tx_queue );

	if ( ! ( netdev->state & NETDEV_OPEN ) ) {
		rc = -ENETUNREACH;
		goto err;
	}
		
	if ( ( rc = netdev->transmit ( netdev, pkb ) ) != 0 )
		goto err;

	return 0;

 err:
	DBGC ( netdev, "NETDEV %p transmission %p failed: %s\n",
	       netdev, pkb, strerror ( rc ) );
	netdev_tx_complete ( netdev, pkb );
	return rc;
}

/**
 * Complete network transmission
 *
 * @v netdev		Network device
 * @v pkb		Packet buffer
 *
 * The packet must currently be in the network device's TX queue.
 */
void netdev_tx_complete ( struct net_device *netdev, struct pk_buff *pkb ) {
	DBGC ( netdev, "NETDEV %p transmission %p complete\n", netdev, pkb );

	list_del ( &pkb->list );
	free_pkb ( pkb );
}

/**
 * Complete network transmission
 *
 * @v netdev		Network device
 *
 * Completes the oldest outstanding packet in the TX queue.
 */
void netdev_tx_complete_next ( struct net_device *netdev ) {
	struct pk_buff *pkb;

	list_for_each_entry ( pkb, &netdev->tx_queue, list ) {
		netdev_tx_complete ( netdev, pkb );
		return;
	}
}

/**
 * Add packet to receive queue
 *
 * @v netdev		Network device
 * @v pkb		Packet buffer
 *
 * The packet is added to the network device's RX queue.  This
 * function takes ownership of the packet buffer.
 */
void netdev_rx ( struct net_device *netdev, struct pk_buff *pkb ) {
	DBGC ( netdev, "NETDEV %p received %p (%p+%zx)\n",
	       netdev, pkb, pkb->data, pkb_len ( pkb ) );
	list_add_tail ( &pkb->list, &netdev->rx_queue );
}

/**
 * Poll for packet on network device
 *
 * @v netdev		Network device
 * @ret True		There are packets present in the receive queue
 * @ret False		There are no packets present in the receive queue
 *
 * Polls the network device for received packets.  Any received
 * packets will be added to the RX packet queue via netdev_rx().
 */
int netdev_poll ( struct net_device *netdev ) {

	if ( netdev->state & NETDEV_OPEN )
		netdev->poll ( netdev );

	return ( ! list_empty ( &netdev->rx_queue ) );
}

/**
 * Remove packet from device's receive queue
 *
 * @v netdev		Network device
 * @ret pkb		Packet buffer, or NULL
 *
 * Removes the first packet from the device's RX queue and returns it.
 * Ownership of the packet is transferred to the caller.
 */
struct pk_buff * netdev_rx_dequeue ( struct net_device *netdev ) {
	struct pk_buff *pkb;

	list_for_each_entry ( pkb, &netdev->rx_queue, list ) {
		list_del ( &pkb->list );
		return pkb;
	}
	return NULL;
}

/**
 * Allocate network device
 *
 * @v priv_size		Size of private data area (net_device::priv)
 * @ret netdev		Network device, or NULL
 *
 * Allocates space for a network device and its private data area.
 */
struct net_device * alloc_netdev ( size_t priv_size ) {
	struct net_device *netdev;

	netdev = calloc ( 1, sizeof ( *netdev ) + priv_size );
	if ( netdev ) {
		INIT_LIST_HEAD ( &netdev->references );
		INIT_LIST_HEAD ( &netdev->tx_queue );
		INIT_LIST_HEAD ( &netdev->rx_queue );
		netdev->priv = ( ( ( void * ) netdev ) + sizeof ( *netdev ) );
	}
	return netdev;
}

/**
 * Register network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 *
 * Adds the network device to the list of network devices.
 */
int register_netdev ( struct net_device *netdev ) {
	
	/* Add to device list */
	list_add_tail ( &netdev->list, &net_devices );
	DBGC ( netdev, "NETDEV %p registered as %s\n",
	       netdev, netdev_name ( netdev ) );

	return 0;
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
int netdev_open ( struct net_device *netdev ) {
	int rc;

	/* Do nothing if device is already open */
	if ( netdev->state & NETDEV_OPEN )
		return 0;

	DBGC ( netdev, "NETDEV %p opening\n", netdev );

	/* Open the device */
	if ( ( rc = netdev->open ( netdev ) ) != 0 )
		return rc;

	/* Mark as opened */
	netdev->state |= NETDEV_OPEN;
	return 0;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
void netdev_close ( struct net_device *netdev ) {
	struct pk_buff *pkb;

	/* Do nothing if device is already closed */
	if ( ! ( netdev->state & NETDEV_OPEN ) )
		return;

	DBGC ( netdev, "NETDEV %p closing\n", netdev );

	/* Close the device */
	netdev->close ( netdev );

	/* Discard any packets in the TX queue */
	while ( ! list_empty ( &netdev->tx_queue ) ) {
		netdev_tx_complete_next ( netdev );
	}

	/* Discard any packets in the RX queue */
	while ( ( pkb = netdev_rx_dequeue ( netdev ) ) ) {
		DBGC ( netdev, "NETDEV %p discarding received %p\n",
		       netdev, pkb );
		free_pkb ( pkb );
	}

	/* Mark as closed */
	netdev->state &= ~NETDEV_OPEN;
}

/**
 * Unregister network device
 *
 * @v netdev		Network device
 *
 * Removes the network device from the list of network devices.
 */
void unregister_netdev ( struct net_device *netdev ) {

	/* Ensure device is closed */
	netdev_close ( netdev );

	/* Kill off any persistent references to this device */
	forget_references ( &netdev->references );

	/* Remove from device list */
	list_del ( &netdev->list );
	DBGC ( netdev, "NETDEV %p unregistered\n", netdev );
}

/**
 * Free network device
 *
 * @v netdev		Network device
 */
void free_netdev ( struct net_device *netdev ) {
	free ( netdev );
}

/**
 * Iterate through network devices
 *
 * @ret netdev		Network device, or NULL
 *
 * This returns the registered network devices in the order of
 * registration.  If no network devices are registered, it will return
 * NULL.
 */
struct net_device * next_netdev ( void ) {
	struct net_device *netdev;

	list_for_each_entry ( netdev, &net_devices, list ) {
		list_del ( &netdev->list );
		list_add_tail ( &netdev->list, &net_devices );
		return netdev;
	}
	return NULL;
}

/**
 * Transmit network-layer packet
 *
 * @v pkb		Packet buffer
 * @v netdev		Network device
 * @v net_protocol	Network-layer protocol
 * @v ll_dest		Destination link-layer address
 * @ret rc		Return status code
 *
 * Prepends link-layer headers to the packet buffer and transmits the
 * packet via the specified network device.  This function takes
 * ownership of the packet buffer.
 */
int net_tx ( struct pk_buff *pkb, struct net_device *netdev,
	     struct net_protocol *net_protocol, const void *ll_dest ) {
	return netdev->ll_protocol->tx ( pkb, netdev, net_protocol, ll_dest );
}

/**
 * Process received network-layer packet
 *
 * @v pkb		Packet buffer
 * @v netdev		Network device
 * @v net_proto		Network-layer protocol, in network-byte order
 * @v ll_source		Source link-layer address
 * @ret rc		Return status code
 */
int net_rx ( struct pk_buff *pkb, struct net_device *netdev,
	     uint16_t net_proto, const void *ll_source ) {
	struct net_protocol *net_protocol;

	/* Hand off to network-layer protocol, if any */
	for ( net_protocol = net_protocols ; net_protocol < net_protocols_end ;
	      net_protocol++ ) {
		if ( net_protocol->net_proto == net_proto ) {
			return net_protocol->rx ( pkb, netdev, ll_source );
		}
	}
	free_pkb ( pkb );
	return 0;
}

/**
 * Single-step the network stack
 *
 * @v process		Network stack process
 *
 * This polls all interfaces for any received packets, and processes
 * at most one packet from the RX queue.
 *
 * We avoid processing all received packets, because processing the
 * received packet can trigger transmission of a new packet (e.g. an
 * ARP response).  Since TX completions will be processed as part of
 * the poll operation, it is easy to overflow small TX queues if
 * multiple packets are processed per poll.
 */
static void net_step ( struct process *process ) {
	struct net_device *netdev;
	struct pk_buff *pkb;

	/* Poll and process each network device */
	list_for_each_entry ( netdev, &net_devices, list ) {

		/* Poll for new packets */
		netdev_poll ( netdev );

		/* Handle at most one received packet per poll */
		if ( ( pkb = netdev_rx_dequeue ( netdev ) ) ) {
			DBGC ( netdev, "NETDEV %p processing %p\n",
			       netdev, pkb );
			netdev->ll_protocol->rx ( pkb, netdev );
		}
	}

	/* Re-schedule ourself */
	schedule ( process );
}

/** Networking stack process */
static struct process net_process = {
	.step = net_step,
};

/** Initialise the networking stack process */
static void init_net ( void ) {
	schedule ( &net_process );
}

INIT_FN ( INIT_PROCESS, init_net, NULL, NULL );
