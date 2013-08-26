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
#include <ipxe/ipv6.h>
#include <ipxe/icmpv6.h>
#include <ipxe/neighbour.h>
#include <ipxe/ndp.h>

/** @file
 *
 * IPv6 neighbour discovery protocol
 *
 */

/**
 * Transmit NDP neighbour solicitation/advertisement packet
 *
 * @v netdev		Network device
 * @v sin6_src		Source socket address
 * @v sin6_dest		Destination socket address
 * @v target		Neighbour target address
 * @v icmp_type		ICMPv6 type
 * @v flags		NDP flags
 * @v option_type	NDP option type
 * @ret rc		Return status code
 */
static int ndp_tx_neighbour ( struct net_device *netdev,
			      struct sockaddr_in6 *sin6_src,
			      struct sockaddr_in6 *sin6_dest,
			      const struct in6_addr *target,
			      unsigned int icmp_type,
			      unsigned int flags,
			      unsigned int option_type ) {
	struct sockaddr_tcpip *st_src =
		( ( struct sockaddr_tcpip * ) sin6_src );
	struct sockaddr_tcpip *st_dest =
		( ( struct sockaddr_tcpip * ) sin6_dest );
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	struct io_buffer *iobuf;
	struct ndp_header *ndp;
	size_t option_len;
	size_t len;
	int rc;

	/* Allocate and populate buffer */
	option_len = ( ( sizeof ( ndp->option[0] ) + ll_protocol->ll_addr_len +
			 NDP_OPTION_BLKSZ - 1 ) &
		       ~( NDP_OPTION_BLKSZ - 1 ) );
	len = ( sizeof ( *ndp ) + option_len );
	iobuf = alloc_iob ( MAX_LL_NET_HEADER_LEN + len );
	if ( ! iobuf )
		return -ENOMEM;
	iob_reserve ( iobuf, MAX_LL_NET_HEADER_LEN );
	ndp = iob_put ( iobuf, len );
	memset ( ndp, 0, len );
	ndp->icmp.type = icmp_type;
	ndp->flags = flags;
	memcpy ( &ndp->target, target, sizeof ( ndp->target ) );
	ndp->option[0].type = option_type;
	ndp->option[0].blocks = ( option_len / NDP_OPTION_BLKSZ );
	memcpy ( ndp->option[0].value, netdev->ll_addr,
		 ll_protocol->ll_addr_len );
	ndp->icmp.chksum = tcpip_chksum ( ndp, len );

	/* Transmit packet */
	if ( ( rc = tcpip_tx ( iobuf, &icmpv6_protocol, st_src, st_dest,
			       netdev, &ndp->icmp.chksum ) ) != 0 ) {
		DBGC ( netdev, "NDP could not transmit packet: %s\n",
		       strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Transmit NDP neighbour discovery request
 *
 * @v netdev		Network device
 * @v net_protocol	Network-layer protocol
 * @v net_dest		Destination network-layer address
 * @v net_source	Source network-layer address
 * @ret rc		Return status code
 */
static int ndp_tx_request ( struct net_device *netdev,
			    struct net_protocol *net_protocol __unused,
			    const void *net_dest, const void *net_source ) {
	struct sockaddr_in6 sin6_src;
	struct sockaddr_in6 sin6_dest;

	/* Construct source address */
	memset ( &sin6_src, 0, sizeof ( sin6_src ) );
	sin6_src.sin6_family = AF_INET6;
	memcpy ( &sin6_src.sin6_addr, net_source,
		 sizeof ( sin6_src.sin6_addr ) );
	sin6_src.sin6_scope_id = htons ( netdev->index );

	/* Construct multicast destination address */
	memset ( &sin6_dest, 0, sizeof ( sin6_dest ) );
	sin6_dest.sin6_family = AF_INET6;
	sin6_dest.sin6_scope_id = htons ( netdev->index );
	ipv6_solicited_node ( &sin6_dest.sin6_addr, net_dest );

	/* Transmit neighbour discovery packet */
	return ndp_tx_neighbour ( netdev, &sin6_src, &sin6_dest, net_dest,
				  ICMPV6_NDP_NEIGHBOUR_SOLICITATION, 0,
				  NDP_OPT_LL_SOURCE );
}

/** NDP neighbour discovery protocol */
struct neighbour_discovery ndp_discovery = {
	.name = "NDP",
	.tx_request = ndp_tx_request,
};

/**
 * Process NDP neighbour solicitation source link-layer address option
 *
 * @v netdev		Network device
 * @v sin6_src		Source socket address
 * @v ndp		NDP packet
 * @v ll_addr		Source link-layer address
 * @v ll_addr_len	Source link-layer address length
 * @ret rc		Return status code
 */
static int ndp_rx_neighbour_solicitation ( struct net_device *netdev,
					   struct sockaddr_in6 *sin6_src,
					   struct ndp_header *ndp __unused,
					   const void *ll_addr,
					   size_t ll_addr_len ) {
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	int rc;

	/* Silently ignore neighbour solicitations for addresses we do
	 * not own.
	 */
	if ( ! ipv6_has_addr ( netdev, &ndp->target ) )
		return 0;

	/* Sanity check */
	if ( ll_addr_len < ll_protocol->ll_addr_len ) {
		DBGC ( netdev, "NDP neighbour solicitation link-layer address "
		       "too short at %zd bytes (min %d bytes)\n",
		       ll_addr_len, ll_protocol->ll_addr_len );
		return -EINVAL;
	}

	/* Create or update neighbour cache entry */
	if ( ( rc = neighbour_define ( netdev, &ipv6_protocol,
				       &sin6_src->sin6_addr,
				       ll_addr ) ) != 0 ) {
		DBGC ( netdev, "NDP could not define %s => %s: %s\n",
		       inet6_ntoa ( &sin6_src->sin6_addr ),
		       ll_protocol->ntoa ( ll_addr ), strerror ( rc ) );
		return rc;
	}

	/* Send neighbour advertisement */
	if ( ( rc = ndp_tx_neighbour ( netdev, NULL, sin6_src, &ndp->target,
				       ICMPV6_NDP_NEIGHBOUR_ADVERTISEMENT,
				       ( NDP_SOLICITED | NDP_OVERRIDE ),
				       NDP_OPT_LL_TARGET ) ) != 0 ) {
		return rc;
	}

	return 0;
}

/**
 * Process NDP neighbour advertisement target link-layer address option
 *
 * @v netdev		Network device
 * @v sin6_src		Source socket address
 * @v ndp		NDP packet
 * @v ll_addr		Target link-layer address
 * @v ll_addr_len	Target link-layer address length
 * @ret rc		Return status code
 */
static int
ndp_rx_neighbour_advertisement ( struct net_device *netdev,
				 struct sockaddr_in6 *sin6_src __unused,
				 struct ndp_header *ndp, const void *ll_addr,
				 size_t ll_addr_len ) {
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	int rc;

	/* Sanity check */
	if ( ll_addr_len < ll_protocol->ll_addr_len ) {
		DBGC ( netdev, "NDP neighbour advertisement link-layer address "
		       "too short at %zd bytes (min %d bytes)\n",
		       ll_addr_len, ll_protocol->ll_addr_len );
		return -EINVAL;
	}

	/* Update neighbour cache entry, if any */
	if ( ( rc = neighbour_update ( netdev, &ipv6_protocol, &ndp->target,
				       ll_addr ) ) != 0 ) {
		DBGC ( netdev, "NDP could not update %s => %s: %s\n",
		       inet6_ntoa ( &ndp->target ),
		       ll_protocol->ntoa ( ll_addr ), strerror ( rc ) );
		return rc;
	}

	return 0;
}

/** An NDP option handler */
struct ndp_option_handler {
	/** ICMPv6 type */
	uint8_t icmp_type;
	/** Option type */
	uint8_t option_type;
	/**
	 * Handle received option
	 *
	 * @v netdev		Network device
	 * @v sin6_src		Source socket address
	 * @v ndp		NDP packet
	 * @v value		Option value
	 * @v len		Option length
	 * @ret rc		Return status code
	 */
	int ( * rx ) ( struct net_device *netdev, struct sockaddr_in6 *sin6_src,
		       struct ndp_header *ndp, const void *value, size_t len );
};

/** NDP option handlers */
static struct ndp_option_handler ndp_option_handlers[] = {
	{
		.icmp_type = ICMPV6_NDP_NEIGHBOUR_SOLICITATION,
		.option_type = NDP_OPT_LL_SOURCE,
		.rx = ndp_rx_neighbour_solicitation,
	},
	{
		.icmp_type = ICMPV6_NDP_NEIGHBOUR_ADVERTISEMENT,
		.option_type = NDP_OPT_LL_TARGET,
		.rx = ndp_rx_neighbour_advertisement,
	},
};

/**
 * Process received NDP option
 *
 * @v netdev		Network device
 * @v sin6_src		Source socket address
 * @v ndp		NDP packet
 * @v type		Option type
 * @v value		Option value
 * @v len		Option length
 * @ret rc		Return status code
 */
static int ndp_rx_option ( struct net_device *netdev,
			   struct sockaddr_in6 *sin6_src,
			   struct ndp_header *ndp, unsigned int type,
			   const void *value, size_t len ) {
	struct ndp_option_handler *handler;
	unsigned int i;

	/* Locate a suitable option handler, if any */
	for ( i = 0 ; i < ( sizeof ( ndp_option_handlers ) /
			    sizeof ( ndp_option_handlers[0] ) ) ; i++ ) {
		handler = &ndp_option_handlers[i];
		if ( ( handler->icmp_type == ndp->icmp.type ) &&
		     ( handler->option_type == type ) ) {
			return handler->rx ( netdev, sin6_src, ndp,
					     value, len );
		}
	}

	/* Silently ignore unknown options as per RFC 4861 */
	return 0;
}

/**
 * Process received NDP packet
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v sin6_src		Source socket address
 * @v sin6_dest		Destination socket address
 * @ret rc		Return status code
 */
static int ndp_rx ( struct io_buffer *iobuf,
		    struct net_device *netdev,
		    struct sockaddr_in6 *sin6_src,
		    struct sockaddr_in6 *sin6_dest __unused ) {
	struct ndp_header *ndp = iobuf->data;
	struct ndp_option *option;
	size_t remaining;
	size_t option_len;
	size_t option_value_len;
	int rc;

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *ndp ) ) {
		DBGC ( netdev, "NDP packet too short at %zd bytes (min %zd "
		       "bytes)\n", iob_len ( iobuf ), sizeof ( *ndp ) );
		rc = -EINVAL;
		goto done;
	}

	/* Search for option */
	option = ndp->option;
	remaining = ( iob_len ( iobuf ) - offsetof ( typeof ( *ndp ), option ));
	while ( remaining ) {

		/* Sanity check */
		if ( ( remaining < sizeof ( *option ) ) ||
		     ( option->blocks == 0 ) ||
		     ( remaining < ( option->blocks * NDP_OPTION_BLKSZ ) ) ) {
			DBGC ( netdev, "NDP bad option length:\n" );
			DBGC_HDA ( netdev, 0, option, remaining );
			rc = -EINVAL;
			goto done;
		}
		option_len = ( option->blocks * NDP_OPTION_BLKSZ );
		option_value_len = ( option_len - sizeof ( *option ) );

		/* Handle option */
		if ( ( rc = ndp_rx_option ( netdev, sin6_src, ndp,
					    option->type, option->value,
					    option_value_len ) ) != 0 ) {
			goto done;
		}

		/* Move to next option */
		option = ( ( ( void * ) option ) + option_len );
		remaining -= option_len;
	}

 done:
	free_iob ( iobuf );
	return rc;
}

/** NDP ICMPv6 handlers */
struct icmpv6_handler ndp_handlers[] __icmpv6_handler = {
	{
		.type = ICMPV6_NDP_NEIGHBOUR_SOLICITATION,
		.rx = ndp_rx,
	},
	{
		.type = ICMPV6_NDP_NEIGHBOUR_ADVERTISEMENT,
		.rx = ndp_rx,
	},
};
