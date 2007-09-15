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
#include <assert.h>
#include <gpxe/if_arp.h>
#include <gpxe/netdevice.h>
#include <gpxe/iobuf.h>
#include <gpxe/infiniband.h>

/** @file
 *
 * Infiniband protocol
 *
 */

/**
 * Find work queue belonging to completion queue
 *
 * @v cq		Completion queue
 * @v qpn		Queue pair number
 * @v is_send		Find send work queue (rather than receive)
 * @ret wq		Work queue, or NULL if not found
 */
struct ib_work_queue * ib_find_wq ( struct ib_completion_queue *cq,
				    unsigned long qpn, int is_send ) {
	struct ib_work_queue *wq;

	list_for_each_entry ( wq, &cq->work_queues, list ) {
		if ( ( wq->qp->qpn == qpn ) && ( wq->is_send == is_send ) )
			return wq;
	}
	return NULL;
}



/** Infiniband broadcast MAC address */
static uint8_t ib_broadcast[IB_ALEN] = { 0xff, };

/**
 * Transmit Infiniband packet
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v net_protocol	Network-layer protocol
 * @v ll_dest		Link-layer destination address
 *
 * Prepends the Infiniband link-layer header and transmits the packet.
 */
static int ib_tx ( struct io_buffer *iobuf, struct net_device *netdev,
		   struct net_protocol *net_protocol, const void *ll_dest ) {
	struct ibhdr *ibhdr = iob_push ( iobuf, sizeof ( *ibhdr ) );

	/* Build Infiniband header */
	ibhdr->proto = net_protocol->net_proto;
	ibhdr->reserved = 0;

	( void ) ll_dest;

	/* Hand off to network device */
	return netdev_tx ( netdev, iobuf );
}

/**
 * Process received Infiniband packet
 *
 * @v iobuf	I/O buffer
 * @v netdev	Network device
 *
 * Strips off the Infiniband link-layer header and passes up to the
 * network-layer protocol.
 */
static int ib_rx ( struct io_buffer *iobuf, struct net_device *netdev ) {
	struct ibhdr *ibhdr = iobuf->data;

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *ibhdr ) ) {
		DBG ( "Infiniband packet too short (%d bytes)\n",
		      iob_len ( iobuf ) );
		free_iob ( iobuf );
		return -EINVAL;
	}

	/* Strip off Infiniband header */
	iob_pull ( iobuf, sizeof ( *ibhdr ) );

	/* Hand off to network-layer protocol */
	return net_rx ( iobuf, netdev, ibhdr->proto, NULL );
}

/**
 * Transcribe Infiniband address
 *
 * @v ll_addr	Link-layer address
 * @ret string	Link-layer address in human-readable format
 */
const char * ib_ntoa ( const void *ll_addr ) {
	static char buf[61];
	const uint8_t *ib_addr = ll_addr;
	unsigned int i;
	char *p = buf;

	for ( i = 0 ; i < IB_ALEN ; i++ ) {
		p += sprintf ( p, ":%02x", ib_addr[i] );
	}
	return ( buf + 1 );
}

/** Infiniband protocol */
struct ll_protocol infiniband_protocol __ll_protocol = {
	.name		= "Infiniband",
	.ll_proto	= htons ( ARPHRD_INFINIBAND ),
	.ll_addr_len	= IB_ALEN,
	.ll_header_len	= IB_HLEN,
	.ll_broadcast	= ib_broadcast,
	.tx		= ib_tx,
	.rx		= ib_rx,
	.ntoa		= ib_ntoa,
};
