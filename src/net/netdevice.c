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
#include <byteswap.h>
#include <string.h>
#include <errno.h>
#include <gpxe/if_ether.h>
#include <gpxe/pkbuff.h>
#include <gpxe/tables.h>
#include <gpxe/process.h>
#include <gpxe/netdevice.h>

/** @file
 *
 * Network device management
 *
 */

/**
 * Static single instance of a network device
 *
 * The gPXE API is designed to accommodate multiple network devices.
 * However, in the interests of code size, the implementation behind
 * the API supports only a single instance of a network device.
 *
 * No code outside of netdevice.c should ever refer directly to @c
 * static_single_netdev.
 *
 * Callers should always check the return status of alloc_netdev(),
 * register_netdev() etc.  In the current implementation this code
 * will be optimised out by the compiler, so there is no penalty.
 */
struct net_device static_single_netdev;

/** Registered network-layer protocols */
static struct net_protocol net_protocols[0] __table_start ( net_protocols );
static struct net_protocol net_protocols_end[0] __table_end ( net_protocols );

/** Network-layer addresses for @c static_single_netdev */
static struct net_address static_single_netdev_addresses[0]
	__table_start ( sgl_netdev_addresses );
static struct net_address static_single_netdev_addresses_end[0]
	__table_end ( sgl_netdev_addresses );

/** Recevied packet queue */
static LIST_HEAD ( rx_queue );

/**
 * Add packet to receive queue
 *
 * @v netdev		Network device
 * @v pkb		Packet buffer
 *
 * The packet is added to the RX queue.  This function takes ownership
 * of the packet buffer.
 */
void netdev_rx ( struct net_device *netdev, struct pk_buff *pkb ) {
	DBG ( "Packet received\n" );
	pkb->ll_protocol = netdev->ll_protocol;
	list_add_tail ( &pkb->list, &rx_queue );
}

/**
 * Identify network protocol
 *
 * @v net_proto		Network-layer protocol, in network-byte order
 * @ret net_protocol	Network-layer protocol, or NULL
 *
 * Identify a network-layer protocol from a protocol number, which
 * must be an ETH_P_XXX constant in network-byte order.
 */
struct net_protocol * find_net_protocol ( uint16_t net_proto ) {
	struct net_protocol *net_protocol;

	for ( net_protocol = net_protocols ; net_protocol < net_protocols_end ;
	      net_protocol++ ) {
		if ( net_protocol->net_proto == net_proto )
			return net_protocol;
	}
	return NULL;
}

/**
 * Identify network device by network-layer address
 *
 * @v net_protocol	Network-layer protocol
 * @v net_addr		Network-layer address
 * @ret netdev		Network device, or NULL
 *
 * Searches through all network devices to find the device with the
 * specified network-layer address.
 *
 * Note that even with a static single network device, this function
 * can still return NULL.
 */
struct net_device *
find_netdev_by_net_addr ( struct net_protocol *net_protocol,
			  void *net_addr ) {
	struct net_address *net_address;
	struct net_device *netdev = &static_single_netdev;
	
	for ( net_address = static_single_netdev_addresses ;
	      net_address < static_single_netdev_addresses_end ;
	      net_address ++ ) {
		if ( ( net_address->net_protocol == net_protocol ) &&
		     ( memcmp ( net_address->net_addr, net_addr,
				net_protocol->net_addr_len ) == 0 ) )
			return netdev;
	}

	return NULL;
}

/**
 * Transmit packet via a network device
 *
 * @v pkb		Packet buffer
 * @v netdev		Network device, or NULL
 * @ret rc		Return status code
 *
 * Transmits the packet via the specified network device.  The packet
 * must begin with a network-layer header, and the @c net_protocol
 * field must have been filled in.  If @c netdev is NULL, the network
 * device is identified via the packet contents, if possible.  This
 * function takes ownership of the packet buffer.
 */
int net_transmit_via ( struct pk_buff *pkb, struct net_device *netdev ) {
	struct net_protocol *net_protocol;
	struct net_header nethdr;
	struct ll_protocol *ll_protocol;
	struct ll_header llhdr;
	int rc;

	/* Perform network-layer routing */
	net_protocol = pkb->net_protocol;
	nethdr.net_protocol = net_protocol;
	if ( ( rc = net_protocol->route ( pkb, &nethdr ) ) != 0 ) {
		DBG ( "Could not route to %s address %s\n",
		      net_protocol->name,
		      net_protocol->ntoa ( nethdr.dest_net_addr ) );
		free_pkb ( pkb );
		return rc;
	}

	/* Identify transmitting network device, if not specified */
	if ( ! netdev ) {
		netdev = find_netdev_by_net_addr ( net_protocol,
						   nethdr.source_net_addr );
		if ( ! netdev ) {
			DBG ( "No network device for %s address %s\n",
			      net_protocol->name,
			      net_protocol->ntoa ( nethdr.source_net_addr ) );
			free_pkb ( pkb );
			return -EHOSTUNREACH;
		}
	}

	/* Perform link-layer routing */
	ll_protocol = netdev->ll_protocol;
	llhdr.ll_protocol = ll_protocol;
	if ( ( rc = ll_protocol->route ( netdev, &nethdr, &llhdr ) ) != 0 ) {
		DBG ( "No link-layer route to %s address %s\n",
		      net_protocol->name,
		      net_protocol->ntoa ( nethdr.dest_net_addr ) );
		free_pkb ( pkb );
		return rc;
	}

	/* Prepend link-layer header */
	pkb_push ( pkb, ll_protocol->ll_header_len );
	ll_protocol->fill_llh ( &llhdr, pkb );

	/* Hand off packet to network device */
	if ( ( rc = netdev->transmit ( netdev, pkb ) ) != 0 ) {
		DBG ( "Device failed to transmit packet\n" );
		return rc;
	}
	
	DBG ( "Packet transmitted\n" );
	return 0;
}

/**
 * Transmit packet
 *
 * @v pkb		Packet buffer
 * @ret rc		Return status code
 *
 * Transmits the packet via the appropriate network device.  This
 * function takes ownership of the packet buffer.
 */
int net_transmit ( struct pk_buff *pkb ) {
	return net_transmit_via ( pkb, NULL );
}

/**
 * Poll for packet on all network devices
 *
 * @ret True		There are packets present in the receive queue
 * @ret False		There are no packets present in the receive queue
 *
 * Polls all network devices for received packets.  Any received
 * packets will be added to the RX packet queue via netdev_rx().
 */
int net_poll ( void ) {
	struct net_device *netdev = &static_single_netdev;

	DBG ( "Polling network\n" );
	netdev->poll ( netdev );

	return ( ! list_empty ( &rx_queue ) );
}

/**
 * Remove packet from receive queue
 *
 * @ret pkb		Packet buffer, or NULL
 *
 * Removes the first packet from the RX queue and returns it.
 * Ownership of the packet is transferred to the caller.
 */
struct pk_buff * net_rx_dequeue ( void ) {
	struct pk_buff *pkb;

	list_for_each_entry ( pkb, &rx_queue, list ) {
		list_del ( &pkb->list );
		return pkb;
	}
	return NULL;
}

/**
 * Process received packet
 *
 * @v pkb		Packet buffer
 * @ret rc		Return status code
 *
 * Processes a packet received from the network (and, usually, removed
 * from the RX queue by net_rx_dequeue()).  This call takes ownership
 * of the packet buffer.
 */
int net_rx_process ( struct pk_buff *pkb ) {
	struct ll_protocol *ll_protocol;
	struct ll_header llhdr;
	struct net_protocol *net_protocol;
	int rc;

	/* Parse link-layer header */
	ll_protocol = pkb->ll_protocol;
	ll_protocol->parse_llh ( pkb, &llhdr );
	
	/* Identify network-layer protocol */
	net_protocol = find_net_protocol ( llhdr.net_proto );
	if ( ! net_protocol ) {
		DBG ( "Unknown network-layer protocol %x\n",
		      ntohs ( llhdr.net_proto ) );
		free_pkb ( pkb );
		return -EPROTONOSUPPORT;
	}
	pkb->net_protocol = net_protocol;
	
	/* Strip off link-layer header */
	pkb_pull ( pkb, ll_protocol->ll_header_len );
	
	/* Hand off to network layer */
	if ( ( rc = net_protocol->rx_process ( pkb ) ) != 0 ) {
		DBG ( "Network-layer protocol dropped packet\n" );
		return rc;
	}

	return 0;
}



/**
 * Single-step the network stack
 *
 * @v process		Network stack process
 *
 * This polls all interfaces for any received packets, and processes
 * any packets that are received during this poll.
 */
static void net_step ( struct process *process ) {
	struct pk_buff *pkb;

	/* Poll for new packets */
	net_poll();

	/* Handle any received packets */
	while ( ( pkb = net_rx_dequeue () ) ) {
		net_rx_process ( pkb );
		DBG ( "Processed received packet\n" );
	}

	/* Re-schedule ourself */
	schedule ( process );
}

/** Networking stack process */
static struct process net_process = {
	.step = net_step,
};

static void init_net ( void ) {
	schedule ( &net_process );
}

#include <init.h>

INIT_FN ( INIT_RPC, init_net, NULL, NULL );
