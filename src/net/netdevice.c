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
 * Identify network protocol
 *
 * @v net_proto		Network-layer protocol, in network-byte order
 * @ret net_protocol	Network-layer protocol, or NULL
 *
 * Identify a network-layer protocol from a protocol number, which
 * must be an ETH_P_XXX constant in network-byte order.
 */
struct net_protocol * net_find_protocol ( uint16_t net_proto ) {
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
struct net_device * net_find_address ( struct net_protocol *net_protocol,
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
 * Transmit packet
 *
 * @v pkb		Packet buffer
 * @ret rc		Return status code
 *
 * Transmits the packet via the appropriate network device.  If this
 * function returns success, it has taken ownership of the packet
 * buffer.
 */
int net_transmit ( struct pk_buff *pkb ) {
	struct net_protocol *net_protocol;
	struct net_header nethdr;
	struct ll_protocol *ll_protocol;
	struct ll_header llhdr;
	struct net_device *netdev;
	int rc;

	/* Perform network-layer routing */
	net_protocol = pkb->net_protocol;
	nethdr.net_protocol = net_protocol;
	if ( ( rc = net_protocol->route ( pkb, &nethdr ) ) != 0 )
		goto err;

	/* Identify transmitting network device */
	netdev = net_find_address ( net_protocol, nethdr.source_net_addr );
	if ( ! netdev )
		goto err;

	/* Perform link-layer routing */
	ll_protocol = netdev->ll_protocol;
	llhdr.ll_protocol = ll_protocol;
	llhdr.net_proto = net_protocol->net_proto;
	memcpy ( llhdr.source_ll_addr, netdev->ll_addr,
		 ll_protocol->ll_addr_len);
	if ( ( rc = ll_protocol->route ( &nethdr, &llhdr ) ) != 0 )
		goto err;

	/* Prepend link-layer header */
	pkb_push ( pkb, ll_protocol->ll_header_len );
	ll_protocol->fill_llh ( &llhdr, pkb );

	/* Transmit packet */
	if ( ( rc = netdev->transmit ( netdev, pkb ) ) != 0 )
		goto err;

	return 0;

 err:
	free_pkb ( pkb );
	return rc;
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

	netdev->poll ( netdev );

	return ( ! list_empty ( &rx_queue ) );
}

/**
 * Add packet to receive queue
 *
 * @v netdev		Network device
 * @v pkb		Packet buffer
 *
 * The packet is added to the RX queue.  Ownership of the packet is
 * transferred to the RX queue; the caller must not touch the packet
 * buffer after calling netdev_rx().
 */
void netdev_rx ( struct net_device *netdev, struct pk_buff *pkb ) {
	pkb->ll_protocol = netdev->ll_protocol;
	list_add_tail ( &pkb->list, &rx_queue );
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

void net_run ( void ) {
	struct pk_buff *pkb;
	struct ll_protocol *ll_protocol;
	struct ll_header llhdr;
	struct net_protocol *net_protocol;

	while ( ( pkb = net_rx_dequeue () ) ) {

		/* Parse link-layer header */
		ll_protocol = pkb->ll_protocol;
		ll_protocol->parse_llh ( pkb, &llhdr );

		/* Identify network-layer protocol */
		net_protocol = net_find_protocol ( llhdr.net_proto );
		if ( ! net_protocol ) {
			DBG ( "Unknown network-layer protocol %02x\n",
			      ntohs ( llhdr.net_proto ) );
			free_pkb ( pkb );
			continue;
		}

		/* Strip off link-layer header */
		pkb_pull ( pkb, ll_protocol->ll_header_len );

		/* Hand off to network layer */
		if ( net_protocol->rx ( pkb ) != 0 ) {
			free_pkb ( pkb );
			continue;
		}
	}
}



