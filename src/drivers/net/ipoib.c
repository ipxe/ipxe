/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <byteswap.h>
#include <errno.h>
#include <gpxe/if_arp.h>
#include <gpxe/iobuf.h>
#include <gpxe/netdevice.h>
#include <gpxe/infiniband.h>
#include <gpxe/ib_pathrec.h>
#include <gpxe/ib_mcast.h>
#include <gpxe/ipoib.h>

/** @file
 *
 * IP over Infiniband
 */

/** Number of IPoIB send work queue entries */
#define IPOIB_NUM_SEND_WQES 2

/** Number of IPoIB receive work queue entries */
#define IPOIB_NUM_RECV_WQES 4

/** Number of IPoIB completion entries */
#define IPOIB_NUM_CQES 8

/** An IPoIB device */
struct ipoib_device {
	/** Network device */
	struct net_device *netdev;
	/** Underlying Infiniband device */
	struct ib_device *ibdev;
	/** Completion queue */
	struct ib_completion_queue *cq;
	/** Queue pair */
	struct ib_queue_pair *qp;
	/** Broadcast MAC */
	struct ipoib_mac broadcast;
	/** Joined to IPv4 broadcast multicast group
	 *
	 * This flag indicates whether or not we have initiated the
	 * join to the IPv4 broadcast multicast group.
	 */
	int broadcast_joined;
	/** IPv4 broadcast multicast group membership */
	struct ib_mc_membership broadcast_membership;
};

/** Broadcast IPoIB address */
static struct ipoib_mac ipoib_broadcast = {
	.qpn = htonl ( IB_QPN_BROADCAST ),
	.gid.u.bytes = 	{ 0xff, 0x12, 0x40, 0x1b, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff },
};

/****************************************************************************
 *
 * IPoIB peer cache
 *
 ****************************************************************************
 */

/**
 * IPoIB peer address
 *
 * The IPoIB link-layer header is only four bytes long and so does not
 * have sufficient room to store IPoIB MAC address(es).  We therefore
 * maintain a cache of MAC addresses identified by a single-byte key,
 * and abuse the spare two bytes within the link-layer header to
 * communicate these MAC addresses between the link-layer code and the
 * netdevice driver.
 */
struct ipoib_peer {
	/** Key */
	uint8_t key;
	/** MAC address */
	struct ipoib_mac mac;
};

/** Number of IPoIB peer cache entries
 *
 * Must be a power of two.
 */
#define IPOIB_NUM_CACHED_PEERS 4

/** IPoIB peer address cache */
static struct ipoib_peer ipoib_peer_cache[IPOIB_NUM_CACHED_PEERS];

/** Oldest IPoIB peer cache entry index */
static unsigned int ipoib_peer_cache_idx = 1;

/**
 * Look up cached peer by key
 *
 * @v key		Peer cache key
 * @ret peer		Peer cache entry, or NULL
 */
static struct ipoib_peer * ipoib_lookup_peer_by_key ( unsigned int key ) {
	struct ipoib_peer *peer;
	unsigned int i;

	for ( i = 0 ; i < IPOIB_NUM_CACHED_PEERS ; i++ ) {
		peer = &ipoib_peer_cache[i];
		if ( peer->key == key )
			return peer;
	}

	if ( key != 0 ) {
		DBG ( "IPoIB warning: peer cache lost track of key %x while "
		      "still in use\n", key );
	}
	return NULL;
}

/**
 * Store GID and QPN in peer cache
 *
 * @v gid		Peer GID
 * @v qpn		Peer QPN
 * @ret peer		Peer cache entry
 */
static struct ipoib_peer * ipoib_cache_peer ( const struct ipoib_mac *mac ) {
	struct ipoib_peer *peer;
	unsigned int key;
	unsigned int i;

	/* Look for existing cache entry */
	for ( i = 0 ; i < IPOIB_NUM_CACHED_PEERS ; i++ ) {
		peer = &ipoib_peer_cache[i];
		if ( memcmp ( &peer->mac, mac, sizeof ( peer->mac ) ) == 0 )
			return peer;
	}

	/* No entry found: create a new one */
	key = ipoib_peer_cache_idx++;
	peer = &ipoib_peer_cache[ key % IPOIB_NUM_CACHED_PEERS ];
	if ( peer->key )
		DBG ( "IPoIB peer %x evicted from cache\n", peer->key );

	memset ( peer, 0, sizeof ( *peer ) );
	peer->key = key;
	memcpy ( &peer->mac, mac, sizeof ( peer->mac ) );
	DBG ( "IPoIB peer %x has MAC %s\n",
	      peer->key, ipoib_ntoa ( &peer->mac ) );
	return peer;
}

/****************************************************************************
 *
 * IPoIB link layer
 *
 ****************************************************************************
 */

/**
 * Add IPoIB link-layer header
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @v ll_dest		Link-layer destination address
 * @v ll_source		Source link-layer address
 * @v net_proto		Network-layer protocol, in network-byte order
 * @ret rc		Return status code
 */
static int ipoib_push ( struct net_device *netdev __unused,
			struct io_buffer *iobuf, const void *ll_dest,
			const void *ll_source __unused, uint16_t net_proto ) {
	struct ipoib_hdr *ipoib_hdr =
		iob_push ( iobuf, sizeof ( *ipoib_hdr ) );
	const struct ipoib_mac *dest_mac = ll_dest;
	const struct ipoib_mac *src_mac = ll_source;
	struct ipoib_peer *dest;
	struct ipoib_peer *src;

	/* Add link-layer addresses to cache */
	dest = ipoib_cache_peer ( dest_mac );
	src = ipoib_cache_peer ( src_mac );

	/* Build IPoIB header */
	ipoib_hdr->proto = net_proto;
	ipoib_hdr->u.peer.dest = dest->key;
	ipoib_hdr->u.peer.src = src->key;

	return 0;
}

/**
 * Remove IPoIB link-layer header
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret ll_dest		Link-layer destination address
 * @ret ll_source	Source link-layer address
 * @ret net_proto	Network-layer protocol, in network-byte order
 * @ret rc		Return status code
 */
static int ipoib_pull ( struct net_device *netdev,
			struct io_buffer *iobuf, const void **ll_dest,
			const void **ll_source, uint16_t *net_proto ) {
	struct ipoib_device *ipoib = netdev->priv;
	struct ipoib_hdr *ipoib_hdr = iobuf->data;
	struct ipoib_peer *dest;
	struct ipoib_peer *source;

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *ipoib_hdr ) ) {
		DBG ( "IPoIB packet too short for link-layer header\n" );
		DBG_HD ( iobuf->data, iob_len ( iobuf ) );
		return -EINVAL;
	}

	/* Strip off IPoIB header */
	iob_pull ( iobuf, sizeof ( *ipoib_hdr ) );

	/* Identify source and destination addresses, and clear
	 * reserved word in IPoIB header
	 */
	dest = ipoib_lookup_peer_by_key ( ipoib_hdr->u.peer.dest );
	source = ipoib_lookup_peer_by_key ( ipoib_hdr->u.peer.src );
	ipoib_hdr->u.reserved = 0;

	/* Fill in required fields */
	*ll_dest = ( dest ? &dest->mac : &ipoib->broadcast );
	*ll_source = ( source ? &source->mac : &ipoib->broadcast );
	*net_proto = ipoib_hdr->proto;

	return 0;
}

/**
 * Initialise IPoIB link-layer address
 *
 * @v hw_addr		Hardware address
 * @v ll_addr		Link-layer address
 */
static void ipoib_init_addr ( const void *hw_addr, void *ll_addr ) {
	const struct ib_gid_half *guid = hw_addr;
	struct ipoib_mac *mac = ll_addr;

	memset ( mac, 0, sizeof ( *mac ) );
	memcpy ( &mac->gid.u.half[1], guid, sizeof ( mac->gid.u.half[1] ) );
}

/**
 * Transcribe IPoIB link-layer address
 *
 * @v ll_addr	Link-layer address
 * @ret string	Link-layer address in human-readable format
 */
const char * ipoib_ntoa ( const void *ll_addr ) {
	static char buf[45];
	const struct ipoib_mac *mac = ll_addr;

	snprintf ( buf, sizeof ( buf ), "%08x:%08x:%08x:%08x:%08x",
		   htonl ( mac->qpn ), htonl ( mac->gid.u.dwords[0] ),
		   htonl ( mac->gid.u.dwords[1] ),
		   htonl ( mac->gid.u.dwords[2] ),
		   htonl ( mac->gid.u.dwords[3] ) );
	return buf;
}

/**
 * Hash multicast address
 *
 * @v af		Address family
 * @v net_addr		Network-layer address
 * @v ll_addr		Link-layer address to fill in
 * @ret rc		Return status code
 */
static int ipoib_mc_hash ( unsigned int af __unused,
			   const void *net_addr __unused,
			   void *ll_addr __unused ) {

	return -ENOTSUP;
}

/** IPoIB protocol */
struct ll_protocol ipoib_protocol __ll_protocol = {
	.name		= "IPoIB",
	.ll_proto	= htons ( ARPHRD_INFINIBAND ),
	.hw_addr_len	= sizeof ( struct ib_gid_half ),
	.ll_addr_len	= IPOIB_ALEN,
	.ll_header_len	= IPOIB_HLEN,
	.push		= ipoib_push,
	.pull		= ipoib_pull,
	.init_addr	= ipoib_init_addr,
	.ntoa		= ipoib_ntoa,
	.mc_hash	= ipoib_mc_hash,
};

/**
 * Allocate IPoIB device
 *
 * @v priv_size		Size of driver private data
 * @ret netdev		Network device, or NULL
 */
struct net_device * alloc_ipoibdev ( size_t priv_size ) {
	struct net_device *netdev;

	netdev = alloc_netdev ( priv_size );
	if ( netdev ) {
		netdev->ll_protocol = &ipoib_protocol;
		netdev->ll_broadcast = ( uint8_t * ) &ipoib_broadcast;
		netdev->max_pkt_len = IB_MAX_PAYLOAD_SIZE;
	}
	return netdev;
}

/****************************************************************************
 *
 * IPoIB network device
 *
 ****************************************************************************
 */

/**
 * Transmit packet via IPoIB network device
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int ipoib_transmit ( struct net_device *netdev,
			    struct io_buffer *iobuf ) {
	struct ipoib_device *ipoib = netdev->priv;
	struct ib_device *ibdev = ipoib->ibdev;
	struct ipoib_hdr *ipoib_hdr;
	struct ipoib_peer *dest;
	struct ib_address_vector av;
	int rc;

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *ipoib_hdr ) ) {
		DBGC ( ipoib, "IPoIB %p buffer too short\n", ipoib );
		return -EINVAL;
	}
	ipoib_hdr = iobuf->data;

	/* Attempting transmission while link is down will put the
	 * queue pair into an error state, so don't try it.
	 */
	if ( ! ib_link_ok ( ibdev ) )
		return -ENETUNREACH;

	/* Identify destination address */
	dest = ipoib_lookup_peer_by_key ( ipoib_hdr->u.peer.dest );
	if ( ! dest )
		return -ENXIO;
	ipoib_hdr->u.reserved = 0;

	/* Construct address vector */
	memset ( &av, 0, sizeof ( av ) );
	av.qpn = ntohl ( dest->mac.qpn );
	av.gid_present = 1;
	memcpy ( &av.gid, &dest->mac.gid, sizeof ( av.gid ) );
	if ( ( rc = ib_resolve_path ( ibdev, &av ) ) != 0 ) {
		/* Path not resolved yet */
		return rc;
	}

	return ib_post_send ( ibdev, ipoib->qp, &av, iobuf );
}

/**
 * Handle IPoIB send completion
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void ipoib_complete_send ( struct ib_device *ibdev __unused,
				  struct ib_queue_pair *qp,
				  struct io_buffer *iobuf, int rc ) {
	struct ipoib_device *ipoib = ib_qp_get_ownerdata ( qp );

	netdev_tx_complete_err ( ipoib->netdev, iobuf, rc );
}

/**
 * Handle IPoIB receive completion
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v av		Address vector, or NULL
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void ipoib_complete_recv ( struct ib_device *ibdev __unused,
				  struct ib_queue_pair *qp,
				  struct ib_address_vector *av,
				  struct io_buffer *iobuf, int rc ) {
	struct ipoib_device *ipoib = ib_qp_get_ownerdata ( qp );
	struct net_device *netdev = ipoib->netdev;
	struct ipoib_hdr *ipoib_hdr;
	struct ipoib_mac ll_src;
	struct ipoib_peer *src;

	if ( rc != 0 ) {
		netdev_rx_err ( netdev, iobuf, rc );
		return;
	}

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( struct ipoib_hdr ) ) {
		DBGC ( ipoib, "IPoIB %p received packet too short to "
		       "contain IPoIB header\n", ipoib );
		DBGC_HD ( ipoib, iobuf->data, iob_len ( iobuf ) );
		netdev_rx_err ( netdev, iobuf, -EIO );
		return;
	}
	ipoib_hdr = iobuf->data;

	/* Parse source address */
	if ( av->gid_present ) {
		ll_src.qpn = htonl ( av->qpn );
		memcpy ( &ll_src.gid, &av->gid, sizeof ( ll_src.gid ) );
		src = ipoib_cache_peer ( &ll_src );
		ipoib_hdr->u.peer.src = src->key;
	}

	/* Hand off to network layer */
	netdev_rx ( netdev, iobuf );
}

/** IPoIB completion operations */
static struct ib_completion_queue_operations ipoib_cq_op = {
	.complete_send = ipoib_complete_send,
	.complete_recv = ipoib_complete_recv,
};

/**
 * Poll IPoIB network device
 *
 * @v netdev		Network device
 */
static void ipoib_poll ( struct net_device *netdev ) {
	struct ipoib_device *ipoib = netdev->priv;
	struct ib_device *ibdev = ipoib->ibdev;

	ib_poll_eq ( ibdev );
}

/**
 * Enable/disable interrupts on IPoIB network device
 *
 * @v netdev		Network device
 * @v enable		Interrupts should be enabled
 */
static void ipoib_irq ( struct net_device *netdev __unused,
			int enable __unused ) {
	/* No implementation */
}

/**
 * Handle IPv4 broadcast multicast group join completion
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v membership	Multicast group membership
 * @v rc		Status code
 * @v mad		Response MAD (or NULL on error)
 */
void ipoib_join_complete ( struct ib_device *ibdev __unused,
			   struct ib_queue_pair *qp __unused,
			   struct ib_mc_membership *membership, int rc,
			   union ib_mad *mad __unused ) {
	struct ipoib_device *ipoib = container_of ( membership,
				   struct ipoib_device, broadcast_membership );

	/* Record join status as link status */
	netdev_link_err ( ipoib->netdev, rc );
}

/**
 * Join IPv4 broadcast multicast group
 *
 * @v ipoib		IPoIB device
 * @ret rc		Return status code
 */
static int ipoib_join_broadcast_group ( struct ipoib_device *ipoib ) {
	int rc;

	if ( ( rc = ib_mcast_join ( ipoib->ibdev, ipoib->qp,
				    &ipoib->broadcast_membership,
				    &ipoib->broadcast.gid,
				    ipoib_join_complete ) ) != 0 ) {
		DBGC ( ipoib, "IPoIB %p could not join broadcast group: %s\n",
		       ipoib, strerror ( rc ) );
		return rc;
	}
	ipoib->broadcast_joined = 1;

	return 0;
}

/**
 * Leave IPv4 broadcast multicast group
 *
 * @v ipoib		IPoIB device
 */
static void ipoib_leave_broadcast_group ( struct ipoib_device *ipoib ) {

	if ( ipoib->broadcast_joined ) {
		ib_mcast_leave ( ipoib->ibdev, ipoib->qp,
				 &ipoib->broadcast_membership );
		ipoib->broadcast_joined = 0;
	}
}

/**
 * Open IPoIB network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int ipoib_open ( struct net_device *netdev ) {
	struct ipoib_device *ipoib = netdev->priv;
	struct ib_device *ibdev = ipoib->ibdev;
	struct ipoib_mac *mac = ( ( struct ipoib_mac * ) netdev->ll_addr );
	int rc;

	/* Open IB device */
	if ( ( rc = ib_open ( ibdev ) ) != 0 ) {
		DBGC ( ipoib, "IPoIB %p could not open device: %s\n",
		       ipoib, strerror ( rc ) );
		goto err_ib_open;
	}

	/* Allocate completion queue */
	ipoib->cq = ib_create_cq ( ibdev, IPOIB_NUM_CQES, &ipoib_cq_op );
	if ( ! ipoib->cq ) {
		DBGC ( ipoib, "IPoIB %p could not allocate completion queue\n",
		       ipoib );
		rc = -ENOMEM;
		goto err_create_cq;
	}

	/* Allocate queue pair */
	ipoib->qp = ib_create_qp ( ibdev, IB_QPT_UD,
				   IPOIB_NUM_SEND_WQES, ipoib->cq,
				   IPOIB_NUM_RECV_WQES, ipoib->cq );
	if ( ! ipoib->qp ) {
		DBGC ( ipoib, "IPoIB %p could not allocate queue pair\n",
		       ipoib );
		rc = -ENOMEM;
		goto err_create_qp;
	}
	ib_qp_set_ownerdata ( ipoib->qp, ipoib );

	/* Update MAC address with QPN */
	mac->qpn = htonl ( ipoib->qp->qpn );

	/* Fill receive rings */
	ib_refill_recv ( ibdev, ipoib->qp );

	/* Fake a link status change to join the broadcast group */
	ipoib_link_state_changed ( ibdev );

	return 0;

	ib_destroy_qp ( ibdev, ipoib->qp );
 err_create_qp:
	ib_destroy_cq ( ibdev, ipoib->cq );
 err_create_cq:
	ib_close ( ibdev );
 err_ib_open:
	return rc;
}

/**
 * Close IPoIB network device
 *
 * @v netdev		Network device
 */
static void ipoib_close ( struct net_device *netdev ) {
	struct ipoib_device *ipoib = netdev->priv;
	struct ib_device *ibdev = ipoib->ibdev;
	struct ipoib_mac *mac = ( ( struct ipoib_mac * ) netdev->ll_addr );

	/* Leave broadcast group */
	ipoib_leave_broadcast_group ( ipoib );

	/* Remove QPN from MAC address */
	mac->qpn = 0;

	/* Tear down the queues */
	ib_destroy_qp ( ibdev, ipoib->qp );
	ib_destroy_cq ( ibdev, ipoib->cq );

	/* Close IB device */
	ib_close ( ibdev );
}

/** IPoIB network device operations */
static struct net_device_operations ipoib_operations = {
	.open		= ipoib_open,
	.close		= ipoib_close,
	.transmit	= ipoib_transmit,
	.poll		= ipoib_poll,
	.irq		= ipoib_irq,
};

/**
 * Handle link status change
 *
 * @v ibdev		Infiniband device
 */
void ipoib_link_state_changed ( struct ib_device *ibdev ) {
	struct net_device *netdev = ib_get_ownerdata ( ibdev );
	struct ipoib_device *ipoib = netdev->priv;
	struct ipoib_mac *mac = ( ( struct ipoib_mac * ) netdev->ll_addr );
	int rc;

	/* Leave existing broadcast group */
	ipoib_leave_broadcast_group ( ipoib );

	/* Update MAC address based on potentially-new GID prefix */
	memcpy ( &mac->gid.u.half[0], &ibdev->gid.u.half[0],
		 sizeof ( mac->gid.u.half[0] ) );

	/* Update broadcast GID based on potentially-new partition key */
	ipoib->broadcast.gid.u.words[2] = htons ( ibdev->pkey );

	/* Set net device link state to reflect Infiniband link state */
	if ( ib_link_ok ( ibdev ) ) {
		netdev_link_up ( netdev );
	} else {
		netdev_link_down ( netdev );
	}

	/* Join new broadcast group */
	if ( ib_link_ok ( ibdev ) &&
	     ( ( rc = ipoib_join_broadcast_group ( ipoib ) ) != 0 ) ) {
		DBGC ( ipoib, "IPoIB %p could not rejoin broadcast group: "
		       "%s\n", ipoib, strerror ( rc ) );
		return;
	}
}

/**
 * Probe IPoIB device
 *
 * @v ibdev		Infiniband device
 * @ret rc		Return status code
 */
int ipoib_probe ( struct ib_device *ibdev ) {
	struct net_device *netdev;
	struct ipoib_device *ipoib;
	int rc;

	/* Allocate network device */
	netdev = alloc_ipoibdev ( sizeof ( *ipoib ) );
	if ( ! netdev )
		return -ENOMEM;
	netdev_init ( netdev, &ipoib_operations );
	ipoib = netdev->priv;
	ib_set_ownerdata ( ibdev, netdev );
	netdev->dev = ibdev->dev;
	memset ( ipoib, 0, sizeof ( *ipoib ) );
	ipoib->netdev = netdev;
	ipoib->ibdev = ibdev;

	/* Extract hardware address */
	memcpy ( netdev->hw_addr, &ibdev->gid.u.half[1],
		 sizeof ( ibdev->gid.u.half[1] ) );

	/* Set default broadcast address */
	memcpy ( &ipoib->broadcast, &ipoib_broadcast,
		 sizeof ( ipoib->broadcast ) );
	netdev->ll_broadcast = ( ( uint8_t * ) &ipoib->broadcast );

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	return 0;

 err_register_netdev:
	netdev_nullify ( netdev );
	netdev_put ( netdev );
	return rc;
}

/**
 * Remove IPoIB device
 *
 * @v ibdev		Infiniband device
 */
void ipoib_remove ( struct ib_device *ibdev ) {
	struct net_device *netdev = ib_get_ownerdata ( ibdev );

	unregister_netdev ( netdev );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}
