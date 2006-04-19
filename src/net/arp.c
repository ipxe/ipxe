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
#include <string.h>
#include <byteswap.h>
#include <errno.h>
#include <gpxe/if_ether.h>
#include <gpxe/if_arp.h>
#include <gpxe/pkbuff.h>
#include <gpxe/llh.h>
#include <gpxe/netdevice.h>
#include <gpxe/arp.h>

/** @file
 *
 * Address Resolution Protocol
 *
 * This file implements the address resolution protocol as defined in
 * RFC826.  The implementation is media-independent and
 * protocol-independent; it is not limited to Ethernet or to IPv4.
 *
 */

/** An ARP cache entry */
struct arp_entry {
	/** Network-layer protocol */
	uint16_t net_proto;
	/** Link-layer protocol */
	uint16_t ll_proto;
	/** Network-layer address */
	uint8_t net_addr[MAX_NET_ADDR_LEN];
	/** Link-layer address */
	uint8_t ll_addr[MAX_LLH_ADDR_LEN];
};

/** Number of entries in the ARP cache
 *
 * This is a global cache, covering all network interfaces,
 * network-layer protocols and link-layer protocols.
 */
#define NUM_ARP_ENTRIES 4

/** The ARP cache */
static struct arp_entry arp_table[NUM_ARP_ENTRIES];
#define arp_table_end &arp_table[NUM_ARP_ENTRIES]

static unsigned int next_new_arp_entry = 0;

/**
 * Find entry in the ARP cache
 *
 * @v ll_proto		Link-layer protocol
 * @v net_proto		Network-layer protocol
 * @v net_addr		Network-layer address
 * @v net_addr_len	Network-layer address length
 * @ret arp		ARP cache entry, or NULL if not found
 *
 */
static struct arp_entry *
arp_find_entry ( uint16_t ll_proto, uint16_t net_proto, const void *net_addr,
		 size_t net_addr_len ) {
	struct arp_entry *arp;

	for ( arp = arp_table ; arp < arp_table_end ; arp++ ) {
		if ( ( arp->ll_proto == ll_proto ) &&
		     ( arp->net_proto == net_proto ) &&
		     ( memcmp ( arp->net_addr, net_addr, net_addr_len ) == 0 ))
			return arp;
	}
	return NULL;
}

/**
 * Look up media-specific link-layer address in the ARP cache
 *
 * @v netdev		Network device
 * @v pkb		Packet buffer
 * @v ll_addr		Buffer to contain link-layer address
 * @ret rc		Return status code
 *
 * The packet buffer must start with a media-independent link-layer
 * header (a struct @c gpxehdr).  This function will use the ARP cache
 * to look up the link-layer address for the media corresponding to
 * @c netdev and the network-layer address as specified in @c gpxehdr.
 *
 * If no address is found in the ARP cache, an ARP request will be
 * transmitted, -ENOENT will be returned, and the packet buffer
 * contents will be undefined.
 */
int arp_resolve ( struct net_device *netdev, struct pk_buff *pkb,
		  void *ll_addr ) {
	struct gpxehdr *gpxehdr = pkb->data;
	const struct arp_entry *arp;
	struct net_interface *netif;
	struct arphdr *arphdr;

	/* Look for existing entry in ARP table */
	arp = arp_find_entry ( netdev->ll_proto, gpxehdr->net_proto,
			       gpxehdr->net_addr, gpxehdr->net_addr_len );
	if ( arp ) {
		memcpy ( ll_addr, arp->ll_addr, netdev->ll_addr_len );
		return 0;
	}

	/* Find interface for this protocol */
	netif = netdev_find_netif ( netdev, gpxehdr->net_proto );
	if ( ! netif )
		return -EAFNOSUPPORT;

	/* Build up ARP request */
	pkb_unput ( pkb, pkb_len ( pkb ) - sizeof ( *gpxehdr ) );
	arphdr = pkb_put ( pkb, sizeof ( *arphdr ) );
	arphdr->ar_hrd = netdev->ll_proto;
	arphdr->ar_hln = netdev->ll_addr_len;
	arphdr->ar_pro = gpxehdr->net_proto;
	arphdr->ar_pln = gpxehdr->net_addr_len;
	arphdr->ar_op = htons ( ARPOP_REQUEST );
	memcpy ( pkb_put ( pkb, netdev->ll_addr_len ),
		 netdev->ll_addr, netdev->ll_addr_len );
	memcpy ( pkb_put ( pkb, netif->net_addr_len ),
		 netif->net_addr, netif->net_addr_len );
	memset ( pkb_put ( pkb, netdev->ll_addr_len ),
		 0xff, netdev->ll_addr_len );
	memcpy ( pkb_put ( pkb, netif->net_addr_len ),
		 gpxehdr->net_addr, netif->net_addr_len );
	pkb_pull ( pkb, sizeof ( *gpxehdr ) );

	/* Locate ARP interface and send ARP request */
	netif = netdev_find_netif ( netdev, htons ( ETH_P_ARP ) );
	assert ( netif != NULL );
	netif_send ( netif, pkb );

	return -ENOENT;
}

/**
 * Process incoming ARP packets
 *
 * @v arp_netif		Network interface for ARP packets
 * @v pkb		Packet buffer
 * @ret rc		Return status code
 *
 * This handles ARP requests and responses as detailed in RFC826.  The
 * method detailed within the RFC is pretty optimised, handling
 * requests and responses with basically a single code path and
 * avoiding the need for extraneous ARP requests; read the RFC for
 * details.
 */
int arp_process ( struct net_interface *arp_netif, struct pk_buff *pkb ) {
	struct arphdr *arphdr = pkb->data;
	struct net_device *netdev = arp_netif->netdev;
	struct net_interface *netif;
	struct arp_entry *arp;
	int merge = 0;

	/* Check for correct link-layer protocol and length */
	if ( ( arphdr->ar_hrd != netdev->ll_proto ) ||
	     ( arphdr->ar_hln != netdev->ll_addr_len ) )
		return 0;

	/* See if we have an interface for this network-layer protocol */
	netif = netdev_find_netif ( netdev, arphdr->ar_pro );
	if ( ! netif )
		return 0;
	if ( arphdr->ar_pln != netif->net_addr_len )
		return 0;

	/* See if we have an entry for this sender, and update it if so */
	arp = arp_find_entry ( arphdr->ar_hrd, arphdr->ar_pro,
			       arp_sender_pa ( arphdr ), arphdr->ar_pln );
	if ( arp ) {
		memcpy ( arp->ll_addr, arp_sender_ha ( arphdr ),
			 arphdr->ar_hln );
		merge = 1;
	}

	/* See if we are the target protocol address */
	if ( memcmp ( arp_target_pa ( arphdr ), netif->net_addr,
		      arphdr->ar_pln ) != 0 )
		return 0;

	/* Create new ARP table entry if necessary */
	if ( ! merge ) {
		arp = &arp_table[next_new_arp_entry++ % NUM_ARP_ENTRIES];
		arp->ll_proto = arphdr->ar_hrd;
		arp->net_proto = arphdr->ar_pro;
		memcpy ( arp->ll_addr, arp_sender_ha ( arphdr ),
			 arphdr->ar_hln );
		memcpy ( arp->net_addr, arp_sender_pa ( arphdr ),
			 arphdr->ar_pln );
	}

	/* If it's not a request, there's nothing more to do */
	if ( arphdr->ar_op != htons ( ARPOP_REQUEST ) )
		return 0;

	/* Change request to a reply, and send it */
	arphdr->ar_op = htons ( ARPOP_REPLY );
	memcpy ( arp_sender_ha ( arphdr ), arp_target_ha ( arphdr ),
		 arphdr->ar_hln + arphdr->ar_pln );
	memcpy ( arp_target_ha ( arphdr ), netdev->ll_addr, arphdr->ar_hln );
	memcpy ( arp_target_pa ( arphdr ), netif->net_addr, arphdr->ar_pln );
	netif_send ( arp_netif, pkb );

	return 0;
}

/**
 * Add media-independent link-layer header
 *
 * @v arp_netif		Network interface for ARP packets
 * @v pkb		Packet buffer
 * @ret rc		Return status code
 */
int arp_add_generic_header ( struct net_interface *arp_netif __unused,
			     struct pk_buff *pkb ) {
	struct arphdr *arphdr = pkb->data;
	struct gpxehdr *gpxehdr;

	/* We're ARP; we always know the raw link-layer address we want */
	gpxehdr = pkb_push ( pkb, sizeof ( *gpxehdr ) );
	gpxehdr->net_proto = htons ( ETH_P_ARP );
	gpxehdr->flags = GPXE_FL_RAW;
	gpxehdr->net_addr_len = arphdr->ar_hln;
	memcpy ( gpxehdr->net_addr, arp_target_ha ( arphdr ), arphdr->ar_hln );
	
	return 0;
}
