/*
 * Copyright (C) 2013 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/in.h>
#include <ipxe/iobuf.h>
#include <ipxe/tcpip.h>
#include <ipxe/icmpv6.h>

/** @file
 *
 * ICMPv6 protocol
 *
 */

/**
 * Process received ICMPv6 echo request packet
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v sin6_src		Source socket address
 * @v sin6_dest		Destination socket address
 * @ret rc		Return status code
 */
static int icmpv6_rx_echo ( struct io_buffer *iobuf,
			    struct net_device *netdev,
			    struct sockaddr_in6 *sin6_src,
			    struct sockaddr_in6 *sin6_dest __unused ) {
	struct sockaddr_tcpip *st_src =
		( ( struct sockaddr_tcpip * ) sin6_src );
	struct icmpv6_echo *echo = iobuf->data;
	size_t len = iob_len ( iobuf );
	int rc;

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *echo ) ) {
		DBGC ( netdev, "ICMPv6 echo request too short at %zd bytes "
		       "(min %zd bytes)\n", iob_len ( iobuf ),
		       sizeof ( *echo ) );
		rc = -EINVAL;
		goto done;
	}
	DBGC ( netdev, "ICMPv6 echo request from %s (id %#04x seq %#04x)\n",
	       inet6_ntoa ( &sin6_dest->sin6_addr ), ntohs ( echo->ident ),
	       ntohs ( echo->sequence ) );

	/* Convert echo request to echo reply and recalculate checksum */
	echo->icmp.type = ICMPV6_ECHO_REPLY;
	echo->icmp.chksum = 0;
	echo->icmp.chksum = tcpip_chksum ( echo, len );

	/* Transmit echo reply */
	if ( ( rc = tcpip_tx ( iob_disown ( iobuf ), &icmpv6_protocol, NULL,
			       st_src, netdev, &echo->icmp.chksum ) ) != 0 ) {
		DBGC ( netdev, "ICMPv6 could not transmit reply: %s\n",
		       strerror ( rc ) );
		goto done;
	}

 done:
	free_iob ( iobuf );
	return rc;
}

/** ICMPv6 echo request handlers */
struct icmpv6_handler icmpv6_echo_handler __icmpv6_handler = {
	.type = ICMPV6_ECHO_REQUEST,
	.rx = icmpv6_rx_echo,
};

/**
 * Identify ICMPv6 handler
 *
 * @v type		ICMPv6 type
 * @ret handler		ICMPv6 handler, or NULL if not found
 */
static struct icmpv6_handler * icmpv6_handler ( unsigned int type ) {
	struct icmpv6_handler *handler;

	for_each_table_entry ( handler, ICMPV6_HANDLERS ) {
		if ( handler->type == type )
			return handler;
	}
	return NULL;
}

/**
 * Process a received packet
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v st_src		Partially-filled source address
 * @v st_dest		Partially-filled destination address
 * @v pshdr_csum	Pseudo-header checksum
 * @ret rc		Return status code
 */
static int icmpv6_rx ( struct io_buffer *iobuf, struct net_device *netdev,
		       struct sockaddr_tcpip *st_src,
		       struct sockaddr_tcpip *st_dest, uint16_t pshdr_csum ) {
	struct sockaddr_in6 *sin6_src = ( ( struct sockaddr_in6 * ) st_src );
	struct sockaddr_in6 *sin6_dest = ( ( struct sockaddr_in6 * ) st_dest );
	struct icmpv6_header *icmp = iobuf->data;
	size_t len = iob_len ( iobuf );
	struct icmpv6_handler *handler;
	unsigned int csum;
	int rc;

	/* Sanity check */
	if ( len < sizeof ( *icmp ) ) {
		DBGC ( netdev, "ICMPv6 packet too short at %zd bytes (min %zd "
		       "bytes)\n", len, sizeof ( *icmp ) );
		rc = -EINVAL;
		goto done;
	}

	/* Verify checksum */
	csum = tcpip_continue_chksum ( pshdr_csum, icmp, len );
	if ( csum != 0 ) {
		DBGC ( netdev, "ICMPv6 checksum incorrect (is %04x, should be "
		       "0000)\n", csum );
		DBGC_HDA ( netdev, 0, icmp, len );
		rc = -EINVAL;
		goto done;
	}

	/* Identify handler */
	handler = icmpv6_handler ( icmp->type );
	if ( ! handler ) {
		DBGC ( netdev, "ICMPv6 unrecognised type %d\n", icmp->type );
		rc = -ENOTSUP;
		goto done;
	}

	/* Pass to handler */
	if ( ( rc = handler->rx ( iob_disown ( iobuf ), netdev, sin6_src,
				  sin6_dest ) ) != 0 ) {
		DBGC ( netdev, "ICMPv6 could not handle type %d: %s\n",
		       icmp->type, strerror ( rc ) );
		goto done;
	}

 done:
	free_iob ( iobuf );
	return rc;
}

/** ICMPv6 TCP/IP protocol */
struct tcpip_protocol icmpv6_protocol __tcpip_protocol = {
	.name = "ICMPv6",
	.rx = icmpv6_rx,
	.tcpip_proto = IP_ICMP6,
};
