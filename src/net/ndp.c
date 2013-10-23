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
	struct ndp_neighbour_header *neigh;
	struct ndp_ll_addr_option *ll_addr_opt;
	size_t option_len;
	size_t len;
	int rc;

	/* Allocate and populate buffer */
	option_len = ( ( sizeof ( *ll_addr_opt ) +
			 ll_protocol->ll_addr_len + NDP_OPTION_BLKSZ - 1 ) &
		       ~( NDP_OPTION_BLKSZ - 1 ) );
	len = ( sizeof ( *neigh ) + option_len );
	iobuf = alloc_iob ( MAX_LL_NET_HEADER_LEN + len );
	if ( ! iobuf )
		return -ENOMEM;
	iob_reserve ( iobuf, MAX_LL_NET_HEADER_LEN );
	neigh = iob_put ( iobuf, len );
	memset ( neigh, 0, len );
	neigh->icmp.type = icmp_type;
	neigh->flags = flags;
	memcpy ( &neigh->target, target, sizeof ( neigh->target ) );
	ll_addr_opt = &neigh->option[0].ll_addr;
	ll_addr_opt->header.type = option_type;
	ll_addr_opt->header.blocks = ( option_len / NDP_OPTION_BLKSZ );
	memcpy ( ll_addr_opt->ll_addr, netdev->ll_addr,
		 ll_protocol->ll_addr_len );
	neigh->icmp.chksum = tcpip_chksum ( neigh, len );

	/* Transmit packet */
	if ( ( rc = tcpip_tx ( iobuf, &icmpv6_protocol, st_src, st_dest,
			       netdev, &neigh->icmp.chksum ) ) != 0 ) {
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
				  ICMPV6_NEIGHBOUR_SOLICITATION, 0,
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
 * @v option		NDP option
 * @v len		NDP option length
 * @ret rc		Return status code
 */
static int
ndp_rx_neighbour_solicitation_ll_source ( struct net_device *netdev,
					  struct sockaddr_in6 *sin6_src,
					  union ndp_header *ndp,
					  union ndp_option *option,
					  size_t len ) {
	struct ndp_neighbour_header *neigh = &ndp->neigh;
	struct ndp_ll_addr_option *ll_addr_opt = &option->ll_addr;
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	int rc;

	/* Silently ignore neighbour solicitations for addresses we do
	 * not own.
	 */
	if ( ! ipv6_has_addr ( netdev, &neigh->target ) )
		return 0;

	/* Sanity check */
	if ( offsetof ( typeof ( *ll_addr_opt ),
			ll_addr[ll_protocol->ll_addr_len] ) > len ) {
		DBGC ( netdev, "NDP neighbour solicitation link-layer address "
		       "option too short at %zd bytes\n", len );
		return -EINVAL;
	}

	/* Create or update neighbour cache entry */
	if ( ( rc = neighbour_define ( netdev, &ipv6_protocol,
				       &sin6_src->sin6_addr,
				       ll_addr_opt->ll_addr ) ) != 0 ) {
		DBGC ( netdev, "NDP could not define %s => %s: %s\n",
		       inet6_ntoa ( &sin6_src->sin6_addr ),
		       ll_protocol->ntoa ( ll_addr_opt->ll_addr ),
		       strerror ( rc ) );
		return rc;
	}

	/* Send neighbour advertisement */
	if ( ( rc = ndp_tx_neighbour ( netdev, NULL, sin6_src, &neigh->target,
				       ICMPV6_NEIGHBOUR_ADVERTISEMENT,
				       ( NDP_NEIGHBOUR_SOLICITED |
					 NDP_NEIGHBOUR_OVERRIDE ),
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
 * @v option		NDP option
 * @v len		NDP option length
 * @ret rc		Return status code
 */
static int
ndp_rx_neighbour_advertisement_ll_target ( struct net_device *netdev,
					   struct sockaddr_in6 *sin6_src
						   __unused,
					   union ndp_header *ndp,
					   union ndp_option *option,
					   size_t len ) {
	struct ndp_neighbour_header *neigh = &ndp->neigh;
	struct ndp_ll_addr_option *ll_addr_opt = &option->ll_addr;
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	int rc;

	/* Sanity check */
	if ( offsetof ( typeof ( *ll_addr_opt ),
			ll_addr[ll_protocol->ll_addr_len] ) > len ) {
		DBGC ( netdev, "NDP neighbour advertisement link-layer address "
		       "option too short at %zd bytes\n", len );
		return -EINVAL;
	}

	/* Update neighbour cache entry, if any */
	if ( ( rc = neighbour_update ( netdev, &ipv6_protocol, &neigh->target,
				       ll_addr_opt->ll_addr ) ) != 0 ) {
		DBGC ( netdev, "NDP could not update %s => %s: %s\n",
		       inet6_ntoa ( &neigh->target ),
		       ll_protocol->ntoa ( ll_addr_opt->ll_addr ),
		       strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Process NDP router advertisement source link-layer address option
 *
 * @v netdev		Network device
 * @v sin6_src		Source socket address
 * @v ndp		NDP packet
 * @v option		NDP option
 * @v len		NDP option length
 * @ret rc		Return status code
 */
static int
ndp_rx_router_advertisement_ll_source ( struct net_device *netdev,
					struct sockaddr_in6 *sin6_src,
					union ndp_header *ndp __unused,
					union ndp_option *option, size_t len ) {
	struct ndp_ll_addr_option *ll_addr_opt = &option->ll_addr;
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	int rc;

	/* Sanity check */
	if ( offsetof ( typeof ( *ll_addr_opt ),
			ll_addr[ll_protocol->ll_addr_len] ) > len ) {
		DBGC ( netdev, "NDP router advertisement link-layer address "
		       "option too short at %zd bytes\n", len );
		return -EINVAL;
	}

	/* Define neighbour cache entry */
	if ( ( rc = neighbour_define ( netdev, &ipv6_protocol,
				       &sin6_src->sin6_addr,
				       ll_addr_opt->ll_addr ) ) != 0 ) {
		DBGC ( netdev, "NDP could not define %s => %s: %s\n",
		       inet6_ntoa ( &sin6_src->sin6_addr ),
		       ll_protocol->ntoa ( ll_addr_opt->ll_addr ),
		       strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Process NDP router advertisement prefix information option
 *
 * @v netdev		Network device
 * @v sin6_src		Source socket address
 * @v ndp		NDP packet
 * @v option		NDP option
 * @v len		NDP option length
 * @ret rc		Return status code
 */
static int
ndp_rx_router_advertisement_prefix ( struct net_device *netdev,
				     struct sockaddr_in6 *sin6_src,
				     union ndp_header *ndp,
				     union ndp_option *option, size_t len ) {
	struct ndp_router_advertisement_header *radv = &ndp->radv;
	struct ndp_prefix_information_option *prefix_opt = &option->prefix;
	struct in6_addr *router = &sin6_src->sin6_addr;
	int rc;

	/* Sanity check */
	if ( sizeof ( *prefix_opt ) > len ) {
		DBGC ( netdev, "NDP router advertisement prefix option too "
		       "short at %zd bytes\n", len );
		return -EINVAL;
	}
	DBGC ( netdev, "NDP found %sdefault router %s ",
	       ( radv->lifetime ? "" : "non-" ),
	       inet6_ntoa ( &sin6_src->sin6_addr ) );
	DBGC ( netdev, "for %s-link %sautonomous prefix %s/%d\n",
	       ( ( prefix_opt->flags & NDP_PREFIX_ON_LINK ) ? "on" : "off" ),
	       ( ( prefix_opt->flags & NDP_PREFIX_AUTONOMOUS ) ? "" : "non-" ),
	       inet6_ntoa ( &prefix_opt->prefix ),
	       prefix_opt->prefix_len );

	/* Perform stateless address autoconfiguration, if applicable */
	if ( ( prefix_opt->flags &
	       ( NDP_PREFIX_ON_LINK | NDP_PREFIX_AUTONOMOUS ) ) ==
	     ( NDP_PREFIX_ON_LINK | NDP_PREFIX_AUTONOMOUS ) ) {
		if ( ( rc = ipv6_slaac ( netdev, &prefix_opt->prefix,
					 prefix_opt->prefix_len,
					 ( radv->lifetime ?
					   router : NULL ) ) ) != 0 ) {
			DBGC ( netdev, "NDP could not autoconfigure prefix %s/"
			       "%d: %s\n", inet6_ntoa ( &prefix_opt->prefix ),
			       prefix_opt->prefix_len, strerror ( rc ) );
			return rc;
		}
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
	 * @v option		NDP option
	 * @ret rc		Return status code
	 */
	int ( * rx ) ( struct net_device *netdev, struct sockaddr_in6 *sin6_src,
		       union ndp_header *ndp, union ndp_option *option,
		       size_t len );
};

/** NDP option handlers */
static struct ndp_option_handler ndp_option_handlers[] = {
	{
		.icmp_type = ICMPV6_NEIGHBOUR_SOLICITATION,
		.option_type = NDP_OPT_LL_SOURCE,
		.rx = ndp_rx_neighbour_solicitation_ll_source,
	},
	{
		.icmp_type = ICMPV6_NEIGHBOUR_ADVERTISEMENT,
		.option_type = NDP_OPT_LL_TARGET,
		.rx = ndp_rx_neighbour_advertisement_ll_target,
	},
	{
		.icmp_type = ICMPV6_ROUTER_ADVERTISEMENT,
		.option_type = NDP_OPT_LL_SOURCE,
		.rx = ndp_rx_router_advertisement_ll_source,
	},
	{
		.icmp_type = ICMPV6_ROUTER_ADVERTISEMENT,
		.option_type = NDP_OPT_PREFIX,
		.rx = ndp_rx_router_advertisement_prefix,
	},
};

/**
 * Process received NDP option
 *
 * @v netdev		Network device
 * @v sin6_src		Source socket address
 * @v ndp		NDP packet
 * @v option		NDP option
 * @v len		Option length
 * @ret rc		Return status code
 */
static int ndp_rx_option ( struct net_device *netdev,
			   struct sockaddr_in6 *sin6_src, union ndp_header *ndp,
			   union ndp_option *option, size_t len ) {
	struct ndp_option_handler *handler;
	unsigned int i;

	/* Locate a suitable option handler, if any */
	for ( i = 0 ; i < ( sizeof ( ndp_option_handlers ) /
			    sizeof ( ndp_option_handlers[0] ) ) ; i++ ) {
		handler = &ndp_option_handlers[i];
		if ( ( handler->icmp_type == ndp->icmp.type ) &&
		     ( handler->option_type == option->header.type ) ) {
			return handler->rx ( netdev, sin6_src, ndp,
					     option, len );
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
 * @v offset		Offset to NDP options
 * @ret rc		Return status code
 */
static int ndp_rx ( struct io_buffer *iobuf,
		    struct net_device *netdev,
		    struct sockaddr_in6 *sin6_src,
		    size_t offset ) {
	union ndp_header *ndp = iobuf->data;
	union ndp_option *option;
	size_t remaining;
	size_t option_len;
	int rc;

	/* Sanity check */
	if ( iob_len ( iobuf ) < offset ) {
		DBGC ( netdev, "NDP packet too short at %zd bytes (min %zd "
		       "bytes)\n", iob_len ( iobuf ), offset );
		rc = -EINVAL;
		goto done;
	}

	/* Search for option */
	option = ( ( ( void * ) ndp ) + offset );
	remaining = ( iob_len ( iobuf ) - offset );
	while ( remaining ) {

		/* Sanity check */
		if ( ( remaining < sizeof ( option->header ) ) ||
		     ( option->header.blocks == 0 ) ||
		     ( remaining < ( option->header.blocks *
				     NDP_OPTION_BLKSZ ) ) ) {
			DBGC ( netdev, "NDP bad option length:\n" );
			DBGC_HDA ( netdev, 0, option, remaining );
			rc = -EINVAL;
			goto done;
		}
		option_len = ( option->header.blocks * NDP_OPTION_BLKSZ );

		/* Handle option */
		if ( ( rc = ndp_rx_option ( netdev, sin6_src, ndp, option,
					    option_len ) ) != 0 )
			goto done;

		/* Move to next option */
		option = ( ( ( void * ) option ) + option_len );
		remaining -= option_len;
	}

	/* Success */
	rc = 0;

 done:
	free_iob ( iobuf );
	return rc;
}

/**
 * Process received NDP neighbour solicitation or advertisement
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v sin6_src		Source socket address
 * @v sin6_dest		Destination socket address
 * @ret rc		Return status code
 */
static int ndp_rx_neighbour ( struct io_buffer *iobuf,
			      struct net_device *netdev,
			      struct sockaddr_in6 *sin6_src,
			      struct sockaddr_in6 *sin6_dest __unused ) {
	union ndp_header *ndp = iobuf->data;
	struct ndp_neighbour_header *neigh = &ndp->neigh;

	return ndp_rx ( iobuf, netdev, sin6_src,
			offsetof ( typeof ( *neigh ), option ) );
}

/**
 * Process received NDP router advertisement
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v sin6_src		Source socket address
 * @v sin6_dest		Destination socket address
 * @ret rc		Return status code
 */
static int
ndp_rx_router_advertisement ( struct io_buffer *iobuf,
			      struct net_device *netdev,
			      struct sockaddr_in6 *sin6_src,
			      struct sockaddr_in6 *sin6_dest __unused ) {
	union ndp_header *ndp = iobuf->data;
	struct ndp_router_advertisement_header *radv = &ndp->radv;

	return ndp_rx ( iobuf, netdev, sin6_src,
			offsetof ( typeof ( *radv ), option ) );
}


/** NDP ICMPv6 handlers */
struct icmpv6_handler ndp_handlers[] __icmpv6_handler = {
	{
		.type = ICMPV6_NEIGHBOUR_SOLICITATION,
		.rx = ndp_rx_neighbour,
	},
	{
		.type = ICMPV6_NEIGHBOUR_ADVERTISEMENT,
		.rx = ndp_rx_neighbour,
	},
	{
		.type = ICMPV6_ROUTER_ADVERTISEMENT,
		.rx = ndp_rx_router_advertisement,
	},
};
