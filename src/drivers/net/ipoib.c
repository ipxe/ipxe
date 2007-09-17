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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include <errno.h>
#include <gpxe/if_arp.h>
#include <gpxe/iobuf.h>
#include <gpxe/netdevice.h>
#include <gpxe/infiniband.h>
#include <gpxe/ipoib.h>

/** @file
 *
 * IP over Infiniband
 */





extern unsigned long hack_ipoib_qkey;
extern struct ib_address_vector hack_ipoib_bcast_av;



/** IPoIB MTU */
#define IPOIB_MTU 2048

/** Number of IPoIB send work queue entries */
#define IPOIB_NUM_SEND_WQES 8

/** Number of IPoIB receive work queue entries */
#define IPOIB_NUM_RECV_WQES 8

/** Number of IPoIB completion entries */
#define IPOIB_NUM_CQES 8

struct ipoib_device {
	struct ib_device *ibdev;
	struct ib_completion_queue *cq;
	struct ib_queue_pair *qp;
	unsigned int rx_fill;
};

/****************************************************************************
 *
 * IPoIB link layer
 *
 ****************************************************************************
 */

/** Broadcast IPoIB address */
static struct ipoib_mac ipoib_broadcast = {
	.gid = { { 0xff, 0x12, 0x40, 0x1b, 0x00, 0x00, 0x00, 0x00,
		   0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff } },
};

/**
 * Transmit IPoIB packet
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v net_protocol	Network-layer protocol
 * @v ll_dest		Link-layer destination address
 *
 * Prepends the IPoIB link-layer header and transmits the packet.
 */
static int ipoib_tx ( struct io_buffer *iobuf, struct net_device *netdev,
		      struct net_protocol *net_protocol,
		      const void *ll_dest ) {
	struct ipoib_hdr *ipoib_hdr =
		iob_push ( iobuf, sizeof ( *ipoib_hdr ) );

	/* Build IPoIB header */
	memcpy ( &ipoib_hdr->pseudo.peer, ll_dest,
		 sizeof ( ipoib_hdr->pseudo.peer ) );
	ipoib_hdr->real.proto = net_protocol->net_proto;
	ipoib_hdr->real.reserved = 0;

	/* Hand off to network device */
	return netdev_tx ( netdev, iobuf );
}

/**
 * Process received IPoIB packet
 *
 * @v iobuf	I/O buffer
 * @v netdev	Network device
 *
 * Strips off the IPoIB link-layer header and passes up to the
 * network-layer protocol.
 */
static int ipoib_rx ( struct io_buffer *iobuf, struct net_device *netdev ) {
	struct ipoib_hdr *ipoib_hdr = iobuf->data;

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *ipoib_hdr ) ) {
		DBG ( "IPoIB packet too short (%d bytes)\n",
		      iob_len ( iobuf ) );
		free_iob ( iobuf );
		return -EINVAL;
	}

	/* Strip off IPoIB header */
	iob_pull ( iobuf, sizeof ( *ipoib_hdr ) );

	/* Hand off to network-layer protocol */
	return net_rx ( iobuf, netdev, ipoib_hdr->real.proto,
			&ipoib_hdr->pseudo.peer );
}

/**
 * Transcribe IPoIB address
 *
 * @v ll_addr	Link-layer address
 * @ret string	Link-layer address in human-readable format
 */
const char * ipoib_ntoa ( const void *ll_addr ) {
	static char buf[61];
	const uint8_t *ipoib_addr = ll_addr;
	unsigned int i;
	char *p = buf;

	for ( i = 0 ; i < IPOIB_ALEN ; i++ ) {
		p += sprintf ( p, ":%02x", ipoib_addr[i] );
	}
	return ( buf + 1 );
}

/** IPoIB protocol */
struct ll_protocol ipoib_protocol __ll_protocol = {
	.name		= "IPoIB",
	.ll_proto	= htons ( ARPHRD_INFINIBAND ),
	.ll_addr_len	= IPOIB_ALEN,
	.ll_header_len	= IPOIB_HLEN,
	.ll_broadcast	= ( uint8_t * ) &ipoib_broadcast,
	.tx		= ipoib_tx,
	.rx		= ipoib_rx,
	.ntoa		= ipoib_ntoa,
};

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
	struct ipoib_pseudo_hdr *ipoib_pshdr = iobuf->data;

	if ( iob_len ( iobuf ) < sizeof ( *ipoib_pshdr ) ) {
		DBGC ( ipoib, "IPoIB %p buffer too short\n", ipoib );
		return -EINVAL;
	}

	iob_pull ( iobuf, ( sizeof ( *ipoib_pshdr ) ) );
	return ib_post_send ( ibdev, ipoib->qp,
			      &hack_ipoib_bcast_av, iobuf );
}

/**
 * Handle IPoIB send completion
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v completion	Completion
 * @v iobuf		I/O buffer
 */
static void ipoib_complete_send ( struct ib_device *ibdev __unused,
				  struct ib_queue_pair *qp,
				  struct ib_completion *completion,
				  struct io_buffer *iobuf ) {
	struct net_device *netdev = qp->owner_priv;

	netdev_tx_complete_err ( netdev, iobuf,
				 ( completion->syndrome ? -EIO : 0 ) );
}

/**
 * Handle IPoIB receive completion
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v completion	Completion
 * @v iobuf		I/O buffer
 */
static void ipoib_complete_recv ( struct ib_device *ibdev __unused,
				  struct ib_queue_pair *qp,
				  struct ib_completion *completion,
				  struct io_buffer *iobuf ) {
	struct net_device *netdev = qp->owner_priv;
	struct ipoib_device *ipoib = netdev->priv;
	struct ib_global_route_header *grh = iobuf->data;
	struct ipoib_pseudo_hdr *ipoib_pshdr;

	if ( completion->syndrome ) {
		netdev_rx_err ( netdev, iobuf, -EIO );
	} else {
		iob_put ( iobuf, completion->len );
		iob_pull ( iobuf, ( sizeof ( *grh ) -
				    sizeof ( *ipoib_pshdr ) ) );
		/* FIXME: fill in a MAC address for the sake of AoE! */
		netdev_rx ( netdev, iobuf );
	}

	ipoib->rx_fill--;
}

/**
 * Refill IPoIB receive ring
 *
 * @v ipoib		IPoIB device
 */
static void ipoib_refill_recv ( struct ipoib_device *ipoib ) {
	struct ib_device *ibdev = ipoib->ibdev;
	struct io_buffer *iobuf;
	int rc;

	while ( ipoib->rx_fill < IPOIB_NUM_RECV_WQES ) {
		iobuf = alloc_iob ( IPOIB_MTU );
		if ( ! iobuf )
			break;
		if ( ( rc = ib_post_recv ( ibdev, ipoib->qp,
					   iobuf ) ) != 0 ) {
			free_iob ( iobuf );
			break;
		}
		ipoib->rx_fill++;
	}
}

/**
 * Poll IPoIB network device
 *
 * @v netdev		Network device
 */
static void ipoib_poll ( struct net_device *netdev ) {
	struct ipoib_device *ipoib = netdev->priv;
	struct ib_device *ibdev = ipoib->ibdev;

	ib_poll_cq ( ibdev, ipoib->cq, ipoib_complete_send,
		     ipoib_complete_recv );
	ipoib_refill_recv ( ipoib );
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
 * Open IPoIB network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int ipoib_open ( struct net_device *netdev ) {
	struct ipoib_device *ipoib = netdev->priv;
	struct ib_device *ibdev = ipoib->ibdev;
	int rc;

	/* Attach to broadcast multicast GID */
	if ( ( rc = ib_mcast_attach ( ibdev, ipoib->qp,
				      &ibdev->broadcast_gid ) ) != 0 ) {
		DBG ( "Could not attach to broadcast GID: %s\n",
		      strerror ( rc ) );
		return rc;
	}

	/* Fill receive ring */
	ipoib_refill_recv ( ipoib );

	return 0;
}

/**
 * Close IPoIB network device
 *
 * @v netdev		Network device
 */
static void ipoib_close ( struct net_device *netdev ) {
	struct ipoib_device *ipoib = netdev->priv;
	struct ib_device *ibdev = ipoib->ibdev;

	/* Detach from broadcast multicast GID */
	ib_mcast_detach ( ibdev, ipoib->qp, &ipoib_broadcast.gid );

	/* FIXME: should probably flush the receive ring */
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
 * Probe IPoIB device
 *
 * @v ibdev		Infiniband device
 * @ret rc		Return status code
 */
int ipoib_probe ( struct ib_device *ibdev ) {
	struct net_device *netdev;
	struct ipoib_device *ipoib;
	struct ipoib_mac *mac;
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
	ipoib->ibdev = ibdev;

	/* Allocate completion queue */
	ipoib->cq = ib_create_cq ( ibdev, IPOIB_NUM_CQES );
	if ( ! ipoib->cq ) {
		DBGC ( ipoib, "IPoIB %p could not allocate completion queue\n",
		       ipoib );
		rc = -ENOMEM;
		goto err_create_cq;
	}

	/* Allocate queue pair */
	ipoib->qp = ib_create_qp ( ibdev, IPOIB_NUM_SEND_WQES,
				   ipoib->cq, IPOIB_NUM_RECV_WQES,
				   ipoib->cq, hack_ipoib_qkey );
	if ( ! ipoib->qp ) {
		DBGC ( ipoib, "IPoIB %p could not allocate queue pair\n",
		       ipoib );
		rc = -ENOMEM;
		goto err_create_qp;
	}
	ipoib->qp->owner_priv = netdev;

	/* Construct MAC address */
	mac = ( ( struct ipoib_mac * ) netdev->ll_addr );
	mac->qpn = htonl ( ipoib->qp->qpn );
	memcpy ( &mac->gid, &ibdev->port_gid, sizeof ( mac->gid ) );

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	return 0;

 err_register_netdev:
	ib_destroy_qp ( ibdev, ipoib->qp );
 err_create_qp:
	ib_destroy_cq ( ibdev, ipoib->cq );
 err_create_cq:
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
