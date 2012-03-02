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

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <byteswap.h>
#include <errno.h>
#include <ipxe/if_ether.h>
#include <ipxe/if_arp.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>
#include <ipxe/list.h>
#include <ipxe/retry.h>
#include <ipxe/timer.h>
#include <ipxe/malloc.h>
#include <ipxe/arp.h>

/** @file
 *
 * Address Resolution Protocol
 *
 * This file implements the address resolution protocol as defined in
 * RFC826.  The implementation is media-independent and
 * protocol-independent; it is not limited to Ethernet or to IPv4.
 *
 */

/** ARP minimum timeout */
#define ARP_MIN_TIMEOUT ( TICKS_PER_SEC / 8 )

/** ARP maximum timeout */
#define ARP_MAX_TIMEOUT ( TICKS_PER_SEC * 3 )

/** An ARP cache entry */
struct arp_entry {
	/** List of ARP cache entries */
	struct list_head list;
	/** Network device */
	struct net_device *netdev;
	/** Network-layer protocol */
	struct net_protocol *net_protocol;
	/** Network-layer destination address */
	uint8_t net_dest[MAX_NET_ADDR_LEN];
	/** Network-layer source address */
	uint8_t net_source[MAX_NET_ADDR_LEN];
	/** Link-layer destination address */
	uint8_t ll_dest[MAX_LL_ADDR_LEN];
	/** Retransmission timer */
	struct retry_timer timer;
	/** Pending I/O buffers */
	struct list_head tx_queue;
};

/** The ARP cache */
static LIST_HEAD ( arp_entries );

struct net_protocol arp_protocol __net_protocol;

static void arp_expired ( struct retry_timer *timer, int over );

/**
 * Create ARP cache entry
 *
 * @v netdev		Network device
 * @v net_protocol	Network-layer protocol
 * @v net_dest		Destination network-layer address
 * @v net_source	Source network-layer address
 * @ret arp		ARP cache entry, or NULL if allocation failed
 */
static struct arp_entry * arp_create ( struct net_device *netdev,
				       struct net_protocol *net_protocol,
				       const void *net_dest,
				       const void *net_source ) {
	struct arp_entry *arp;

	/* Allocate entry and add to cache */
	arp = zalloc ( sizeof ( *arp ) );
	if ( ! arp )
		return NULL;

	/* Initialise entry and add to cache */
	arp->netdev = netdev_get ( netdev );
	arp->net_protocol = net_protocol;
	memcpy ( arp->net_dest, net_dest,
		 net_protocol->net_addr_len );
	memcpy ( arp->net_source, net_source,
		 net_protocol->net_addr_len );
	timer_init ( &arp->timer, arp_expired, NULL );
	arp->timer.min_timeout = ARP_MIN_TIMEOUT;
	arp->timer.max_timeout = ARP_MAX_TIMEOUT;
	INIT_LIST_HEAD ( &arp->tx_queue );
	list_add ( &arp->list, &arp_entries );

	/* Start timer running to trigger initial transmission */
	start_timer_nodelay ( &arp->timer );

	DBGC ( arp, "ARP %p %s %s %s created\n", arp, netdev->name,
	       net_protocol->name, net_protocol->ntoa ( net_dest ) );
	return arp;
}

/**
 * Find entry in the ARP cache
 *
 * @v netdev		Network device
 * @v net_protocol	Network-layer protocol
 * @v net_dest		Destination network-layer address
 * @ret arp		ARP cache entry, or NULL if not found
 */
static struct arp_entry * arp_find ( struct net_device *netdev,
				     struct net_protocol *net_protocol,
				     const void *net_dest ) {
	struct arp_entry *arp;

	list_for_each_entry ( arp, &arp_entries, list ) {
		if ( ( arp->netdev == netdev ) &&
		     ( arp->net_protocol == net_protocol ) &&
		     ( memcmp ( arp->net_dest, net_dest,
				net_protocol->net_addr_len ) == 0 ) ) {

			/* Move to start of cache */
			list_del ( &arp->list );
			list_add ( &arp->list, &arp_entries );

			return arp;
		}
	}
	return NULL;
}

/**
 * Destroy ARP cache entry
 *
 * @v arp		ARP cache entry
 * @v rc		Reason for destruction
 */
static void arp_destroy ( struct arp_entry *arp, int rc ) {
	struct net_device *netdev = arp->netdev;
	struct net_protocol *net_protocol = arp->net_protocol;
	struct io_buffer *iobuf;
	struct io_buffer *tmp;

	/* Stop timer */
	stop_timer ( &arp->timer );

	/* Discard any outstanding I/O buffers */
	list_for_each_entry_safe ( iobuf, tmp, &arp->tx_queue, list ) {
		DBGC2 ( arp, "ARP %p %s %s %s discarding deferred packet: "
			"%s\n", arp, netdev->name, net_protocol->name,
			net_protocol->ntoa ( arp->net_dest ), strerror ( rc ) );
		list_del ( &iobuf->list );
		netdev_tx_err ( arp->netdev, iobuf, rc );
	}

	DBGC ( arp, "ARP %p %s %s %s destroyed: %s\n", arp, netdev->name,
	       net_protocol->name, net_protocol->ntoa ( arp->net_dest ),
	       strerror ( rc ) );

	/* Drop reference to network device, remove from cache and free */
	netdev_put ( arp->netdev );
	list_del ( &arp->list );
	free ( arp );
}

/**
 * Test if ARP cache entry has a valid link-layer address
 *
 * @v arp		ARP cache entry
 * @ret resolved	ARP cache entry is resolved
 */
static inline int arp_resolved ( struct arp_entry *arp ) {
	return ( ! timer_running ( &arp->timer ) );
}

/**
 * Transmit packet, determining link-layer address via ARP
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v net_protocol	Network-layer protocol
 * @v net_dest		Destination network-layer address
 * @v net_source	Source network-layer address
 * @v ll_source		Source link-layer address
 * @ret rc		Return status code
 */
int arp_tx ( struct io_buffer *iobuf, struct net_device *netdev,
	     struct net_protocol *net_protocol, const void *net_dest,
	     const void *net_source, const void *ll_source ) {
	struct arp_entry *arp;

	/* Find or create ARP cache entry */
	arp = arp_find ( netdev, net_protocol, net_dest );
	if ( ! arp ) {
		arp = arp_create ( netdev, net_protocol, net_dest,
				   net_source );
		if ( ! arp )
			return -ENOMEM;
	}

	/* If a link-layer address is available then transmit
	 * immediately, otherwise queue for later transmission.
	 */
	if ( arp_resolved ( arp ) ) {
		return net_tx ( iobuf, netdev, net_protocol, arp->ll_dest,
				ll_source );
	} else {
		DBGC2 ( arp, "ARP %p %s %s %s deferring packet\n",
			arp, netdev->name, net_protocol->name,
			net_protocol->ntoa ( net_dest ) );
		list_add_tail ( &iobuf->list, &arp->tx_queue );
		return -EAGAIN;
	}
}

/**
 * Update ARP cache entry
 *
 * @v arp		ARP cache entry
 * @v ll_dest		Destination link-layer address
 */
static void arp_update ( struct arp_entry *arp, const void *ll_dest ) {
	struct net_device *netdev = arp->netdev;
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	struct net_protocol *net_protocol = arp->net_protocol;
	struct io_buffer *iobuf;
	struct io_buffer *tmp;
	int rc;

	DBGC ( arp, "ARP %p %s %s %s updated => %s\n", arp, netdev->name,
	       net_protocol->name, net_protocol->ntoa ( arp->net_dest ),
	       ll_protocol->ntoa ( ll_dest ) );

	/* Fill in link-layer address */
	memcpy ( arp->ll_dest, ll_dest, ll_protocol->ll_addr_len );

	/* Stop retransmission timer */
	stop_timer ( &arp->timer );

	/* Transmit any packets in queue */
	list_for_each_entry_safe ( iobuf, tmp, &arp->tx_queue, list ) {
		DBGC2 ( arp, "ARP %p %s %s %s transmitting deferred packet\n",
			arp, netdev->name, net_protocol->name,
			net_protocol->ntoa ( arp->net_dest ) );
		list_del ( &iobuf->list );
		if ( ( rc = net_tx ( iobuf, netdev, net_protocol, ll_dest,
				     netdev->ll_addr ) ) != 0 ) {
			DBGC ( arp, "ARP %p could not transmit deferred "
			       "packet: %s\n", arp, strerror ( rc ) );
			/* Ignore error and continue */
		}
	}
}

/**
 * Handle ARP timer expiry
 *
 * @v timer		Retry timer
 * @v fail		Failure indicator
 */
static void arp_expired ( struct retry_timer *timer, int fail ) {
	struct arp_entry *arp = container_of ( timer, struct arp_entry, timer );
	struct net_device *netdev = arp->netdev;
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	struct net_protocol *net_protocol = arp->net_protocol;
	struct io_buffer *iobuf;
	struct arphdr *arphdr;
	int rc;

	/* If we have failed, destroy the cache entry */
	if ( fail ) {
		arp_destroy ( arp, -ETIMEDOUT );
		return;
	}

	/* Restart the timer */
	start_timer ( &arp->timer );

	/* Allocate ARP packet */
	iobuf = alloc_iob ( MAX_LL_HEADER_LEN + sizeof ( *arphdr ) +
			    ( 2 * ( MAX_LL_ADDR_LEN + MAX_NET_ADDR_LEN ) ) );
	if ( ! iobuf ) {
		/* Leave timer running and try again later */
		return;
	}
	iob_reserve ( iobuf, MAX_LL_HEADER_LEN );

	/* Build up ARP request */
	arphdr = iob_put ( iobuf, sizeof ( *arphdr ) );
	arphdr->ar_hrd = ll_protocol->ll_proto;
	arphdr->ar_hln = ll_protocol->ll_addr_len;
	arphdr->ar_pro = net_protocol->net_proto;
	arphdr->ar_pln = net_protocol->net_addr_len;
	arphdr->ar_op = htons ( ARPOP_REQUEST );
	memcpy ( iob_put ( iobuf, ll_protocol->ll_addr_len ),
		 netdev->ll_addr, ll_protocol->ll_addr_len );
	memcpy ( iob_put ( iobuf, net_protocol->net_addr_len ),
		 arp->net_source, net_protocol->net_addr_len );
	memset ( iob_put ( iobuf, ll_protocol->ll_addr_len ),
		 0, ll_protocol->ll_addr_len );
	memcpy ( iob_put ( iobuf, net_protocol->net_addr_len ),
		 arp->net_dest, net_protocol->net_addr_len );

	/* Transmit ARP request */
	if ( ( rc = net_tx ( iobuf, netdev, &arp_protocol,
			     netdev->ll_broadcast, netdev->ll_addr ) ) != 0 ) {
		DBGC ( arp, "ARP %p could not transmit request: %s\n",
		       arp, strerror ( rc ) );
		return;
	}
}

/**
 * Identify ARP protocol
 *
 * @v net_proto			Network-layer protocol, in network-endian order
 * @ret arp_net_protocol	ARP protocol, or NULL
 *
 */
static struct arp_net_protocol * arp_find_protocol ( uint16_t net_proto ) {
	struct arp_net_protocol *arp_net_protocol;

	for_each_table_entry ( arp_net_protocol, ARP_NET_PROTOCOLS ) {
		if ( arp_net_protocol->net_protocol->net_proto == net_proto ) {
			return arp_net_protocol;
		}
	}
	return NULL;
}

/**
 * Process incoming ARP packets
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v ll_source		Link-layer source address
 * @v flags		Packet flags
 * @ret rc		Return status code
 */
static int arp_rx ( struct io_buffer *iobuf, struct net_device *netdev,
		    const void *ll_dest __unused,
		    const void *ll_source __unused,
		    unsigned int flags __unused ) {
	struct arphdr *arphdr = iobuf->data;
	struct arp_net_protocol *arp_net_protocol;
	struct net_protocol *net_protocol;
	struct ll_protocol *ll_protocol;
	struct arp_entry *arp;
	int rc;

	/* Identify network-layer and link-layer protocols */
	arp_net_protocol = arp_find_protocol ( arphdr->ar_pro );
	if ( ! arp_net_protocol ) {
		rc = -EPROTONOSUPPORT;
		goto done;
	}
	net_protocol = arp_net_protocol->net_protocol;
	ll_protocol = netdev->ll_protocol;

	/* Sanity checks */
	if ( ( arphdr->ar_hrd != ll_protocol->ll_proto ) ||
	     ( arphdr->ar_hln != ll_protocol->ll_addr_len ) ||
	     ( arphdr->ar_pln != net_protocol->net_addr_len ) ) {
		rc = -EINVAL;
		goto done;
	}

	/* See if we have an entry for this sender, and update it if so */
	arp = arp_find ( netdev, net_protocol, arp_sender_pa ( arphdr ) );
	if ( arp ) {
		arp_update ( arp, arp_sender_ha ( arphdr ) );
	}

	/* If it's not a request, there's nothing more to do */
	if ( arphdr->ar_op != htons ( ARPOP_REQUEST ) ) {
		rc = 0;
		goto done;
	}

	/* See if we own the target protocol address */
	if ( arp_net_protocol->check ( netdev, arp_target_pa ( arphdr ) ) != 0){
		rc = 0;
		goto done;
	}

	/* Change request to a reply */
	DBGC ( netdev, "ARP reply %s %s %s => %s %s\n",
	       netdev->name, net_protocol->name,
	       net_protocol->ntoa ( arp_target_pa ( arphdr ) ),
	       ll_protocol->name, ll_protocol->ntoa ( netdev->ll_addr ) );
	arphdr->ar_op = htons ( ARPOP_REPLY );
	memswap ( arp_sender_ha ( arphdr ), arp_target_ha ( arphdr ),
		 arphdr->ar_hln + arphdr->ar_pln );
	memcpy ( arp_sender_ha ( arphdr ), netdev->ll_addr, arphdr->ar_hln );

	/* Send reply */
	if ( ( rc = net_tx ( iob_disown ( iobuf ), netdev, &arp_protocol,
			     arp_target_ha ( arphdr ),
			     netdev->ll_addr ) ) != 0 ) {
		DBGC ( netdev, "ARP could not transmit reply via %s: %s\n",
		       netdev->name, strerror ( rc ) );
		goto done;
	}

	/* Success */
	rc = 0;

 done:
	free_iob ( iobuf );
	return rc;
}

/**
 * Transcribe ARP address
 *
 * @v net_addr	ARP address
 * @ret string	"<ARP>"
 *
 * This operation is meaningless for the ARP protocol.
 */
static const char * arp_ntoa ( const void *net_addr __unused ) {
	return "<ARP>";
}

/** ARP protocol */
struct net_protocol arp_protocol __net_protocol = {
	.name = "ARP",
	.net_proto = htons ( ETH_P_ARP ),
	.rx = arp_rx,
	.ntoa = arp_ntoa,
};

/**
 * Update ARP cache on network device creation
 *
 * @v netdev		Network device
 */
static int arp_probe ( struct net_device *netdev __unused ) {
	/* Nothing to do */
	return 0;
}

/**
 * Update ARP cache on network device state change or removal
 *
 * @v netdev		Network device
 */
static void arp_flush ( struct net_device *netdev ) {
	struct arp_entry *arp;
	struct arp_entry *tmp;

	/* Remove all ARP cache entries when a network device is closed */
	if ( ! netdev_is_open ( netdev ) ) {
		list_for_each_entry_safe ( arp, tmp, &arp_entries, list )
			arp_destroy ( arp, -ENODEV );
	}
}

/** ARP driver (for net device notifications) */
struct net_driver arp_net_driver __net_driver = {
	.name = "ARP",
	.probe = arp_probe,
	.notify = arp_flush,
	.remove = arp_flush,
};

/**
 * Discard some cached ARP entries
 *
 * @ret discarded	Number of cached items discarded
 */
static unsigned int arp_discard ( void ) {
	struct arp_entry *arp;

	/* Drop oldest cache entry, if any */
	list_for_each_entry_reverse ( arp, &arp_entries, list ) {
		arp_destroy ( arp, -ENOBUFS );
		return 1;
	}

	return 0;
}

/** ARP cache discarder */
struct cache_discarder arp_cache_discarder __cache_discarder = {
	.discard = arp_discard,
};
