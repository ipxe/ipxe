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
#include <errno.h>
#include <gpxe/if_ether.h>
#include <gpxe/pkbuff.h>
#include <gpxe/netdevice.h>

/** @file
 *
 * Network devices and network interfaces
 *
 */

/** List of all registered network devices */
static LIST_HEAD ( net_devices );

/**
 * Register network device
 *
 * @v netdev	Network device
 * @ret rc	Return status code
 *
 * Adds the network device to the list of network devices.
 */
int register_netdevice ( struct net_device *netdev ) {
	list_add ( &netdev->devices, &net_devices );
	return 0;
}

/**
 * Unregister network device
 *
 * @v netdev	Network device
 *
 * Removes the network device from the list of network devices.
 */
void unregister_netdevice ( struct net_device *netdev ) {
	list_del ( &netdev->devices );
}

/**
 * Transmit packet via network device
 *
 * @v netdev	Network device
 * @v pkb	Packet buffer
 * @ret rc	Return status code
 *
 * Transmits the packet via the network device.  The @c pkb link-layer
 * metadata must already have been filled in, and space for the
 * link-layer header must already be present in the packet buffer.
 */
int netdev_send ( struct net_device *netdev, struct pk_buff *pkb ) {
	int rc;

	if ( pkb->net_proto != ETH_P_RAW ) {
		if ( ( rc = netdev->build_llh ( netdev, pkb ) ) != 0 )
			return rc;
	}
	return netdev->transmit ( netdev, pkb );
}

/**
 * Poll for packet on network device
 *
 * @v netdev	Network device
 * @v pkb	Packet buffer
 * @ret rc	Return status code
 *
 * Polls the network device for a packet.  If a packet is available,
 * it will be added to the packet buffer, and the link-layer metadata
 * fields in @c pkb will be filled in.
 */
int netdev_poll ( struct net_device *netdev, struct pk_buff *pkb ) {
	int rc;

	if ( ( rc = netdev->poll ( netdev, pkb ) ) != 0 )
		return rc;
	return netdev->parse_llh ( netdev, pkb );
}

/**
 * Transmit packet via network interface
 *
 * @v netif	Network interface
 * @v pkb	Packet buffer
 * @ret rc	Return status code
 *
 * Transmits the packet via the network interface.  The packet must
 * start with a network-layer header (e.g. an IP header, for an IP
 * interface).  The packet contents are undefined on return.
 */
int netif_send ( struct net_interface *netif, struct pk_buff *pkb ) {
	struct net_device *netdev = netif->netdev;
	int rc;

	if ( ( rc = netif->add_llh_metadata ( netif, pkb ) ) != 0 )
		return rc;
	pkb_push ( pkb, netdev->ll_hlen );
	return netdev_send ( netdev, pkb );
}

/**
 * Process received packet
 *
 * @v netif	Network interface
 * @v pkb	Packet buffer
 * @ret rc	Return status code
 *
 * Processes a packet received via netdev_poll().  The interface
 * corresponding to the network-layer protocol is identified, the
 * link-layer header is stripped from the packet and the packet is
 * passed to the net_interface::rx_packet() method.
 */
int netdev_rx_packet ( struct net_device *netdev, struct pk_buff *pkb ) {
	struct net_interface *netif;

	netif = netdev_find_netif ( netdev, pkb->net_proto );
	if ( ! netif )
		return -EAFNOSUPPORT;

	pkb_pull ( pkb, netdev->ll_hlen );
	return netif->rx_packet ( netif, pkb );
}

/**
 * Poll for packet on all network devices
 *
 * @v pkb	Packet buffer
 * @ret netdev	Network device
 * @ret rc	Return status code
 *
 * Polls all network devices for a packet.  If a packet is available
 * on any interface, @c netdev will be filled in and the packet will
 * be received as per netdev_poll().
 */
int net_poll ( struct pk_buff *pkb, struct net_device **netdev ) {
	int rc;

	list_for_each_entry ( (*netdev), &net_devices, devices ) {
		if ( ( rc = netdev_poll ( *netdev, pkb ) ) == 0 )
			return rc;
	}

	return -EAGAIN;
}
