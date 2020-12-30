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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/in.h>
#include <ipxe/iobuf.h>
#include <ipxe/tcpip.h>
#include <ipxe/ipv6.h>
#include <ipxe/icmpv6.h>
#include <ipxe/neighbour.h>
#include <ipxe/dhcpv6.h>
#include <ipxe/timer.h>
#include <ipxe/ndp.h>

/** @file
 *
 * IPv6 neighbour discovery protocol
 *
 */

/** Router discovery minimum timeout */
#define IPV6CONF_MIN_TIMEOUT ( TICKS_PER_SEC / 8 )

/** Router discovery maximum timeout */
#define IPV6CONF_MAX_TIMEOUT ( TICKS_PER_SEC * 3 )

static struct ipv6conf * ipv6conf_demux ( struct net_device *netdev );
static int
ipv6conf_rx_router_advertisement ( struct net_device *netdev,
				   struct in6_addr *router,
				   struct ndp_router_advertisement_header *radv,
				   size_t len );

/**
 * Transmit NDP packet with link-layer address option
 *
 * @v netdev		Network device
 * @v sin6_src		Source socket address
 * @v sin6_dest		Destination socket address
 * @v data		NDP header
 * @v len		Size of NDP header
 * @v option_type	NDP option type
 * @ret rc		Return status code
 */
static int ndp_tx_ll_addr ( struct net_device *netdev,
			    struct sockaddr_in6 *sin6_src,
			    struct sockaddr_in6 *sin6_dest,
			    const void *data, size_t len,
			    unsigned int option_type ) {
	struct sockaddr_tcpip *st_src =
		( ( struct sockaddr_tcpip * ) sin6_src );
	struct sockaddr_tcpip *st_dest =
		( ( struct sockaddr_tcpip * ) sin6_dest );
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	struct io_buffer *iobuf;
	struct ndp_ll_addr_option *ll_addr_opt;
	union ndp_header *ndp;
	size_t option_len;
	int rc;

	/* Allocate and populate buffer */
	option_len = ( ( sizeof ( *ll_addr_opt ) +
			 ll_protocol->ll_addr_len + NDP_OPTION_BLKSZ - 1 ) &
		       ~( NDP_OPTION_BLKSZ - 1 ) );
	iobuf = alloc_iob ( MAX_LL_NET_HEADER_LEN + len + option_len );
	if ( ! iobuf )
		return -ENOMEM;
	iob_reserve ( iobuf, MAX_LL_NET_HEADER_LEN );
	memcpy ( iob_put ( iobuf, len ), data, len );
	ll_addr_opt = iob_put ( iobuf, option_len );
	ll_addr_opt->header.type = option_type;
	ll_addr_opt->header.blocks = ( option_len / NDP_OPTION_BLKSZ );
	memcpy ( ll_addr_opt->ll_addr, netdev->ll_addr,
		 ll_protocol->ll_addr_len );
	ndp = iobuf->data;
	ndp->icmp.chksum = tcpip_chksum ( ndp, ( len + option_len ) );

	/* Transmit packet */
	if ( ( rc = tcpip_tx ( iobuf, &icmpv6_protocol, st_src, st_dest,
			       netdev, &ndp->icmp.chksum ) ) != 0 ) {
		DBGC ( netdev, "NDP %s could not transmit packet: %s\n",
		       netdev->name, strerror ( rc ) );
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
	struct ndp_neighbour_header neigh;
	int rc;

	/* Construct source address */
	memset ( &sin6_src, 0, sizeof ( sin6_src ) );
	sin6_src.sin6_family = AF_INET6;
	memcpy ( &sin6_src.sin6_addr, net_source,
		 sizeof ( sin6_src.sin6_addr ) );

	/* Construct multicast destination address */
	memset ( &sin6_dest, 0, sizeof ( sin6_dest ) );
	sin6_dest.sin6_family = AF_INET6;
	sin6_dest.sin6_scope_id = netdev->index;
	ipv6_solicited_node ( &sin6_dest.sin6_addr, net_dest );

	/* Construct neighbour header */
	memset ( &neigh, 0, sizeof ( neigh ) );
	neigh.icmp.type = ICMPV6_NEIGHBOUR_SOLICITATION;
	memcpy ( &neigh.target, net_dest, sizeof ( neigh.target ) );

	/* Transmit neighbour discovery packet */
	if ( ( rc = ndp_tx_ll_addr ( netdev, &sin6_src, &sin6_dest, &neigh,
				     sizeof ( neigh ),
				     NDP_OPT_LL_SOURCE ) ) != 0 )
		return rc;

	return 0;
}

/** NDP neighbour discovery protocol */
struct neighbour_discovery ndp_discovery = {
	.name = "NDP",
	.tx_request = ndp_tx_request,
};

/**
 * Transmit NDP router solicitation
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int ndp_tx_router_solicitation ( struct net_device *netdev ) {
	struct ndp_router_solicitation_header rsol;
	struct sockaddr_in6 sin6_dest;
	int rc;

	/* Construct multicast destination address */
	memset ( &sin6_dest, 0, sizeof ( sin6_dest ) );
	sin6_dest.sin6_family = AF_INET6;
	sin6_dest.sin6_scope_id = netdev->index;
	ipv6_all_routers ( &sin6_dest.sin6_addr );

	/* Construct router solicitation */
	memset ( &rsol, 0, sizeof ( rsol ) );
	rsol.icmp.type = ICMPV6_ROUTER_SOLICITATION;

	/* Transmit packet */
	if ( ( rc = ndp_tx_ll_addr ( netdev, NULL, &sin6_dest, &rsol,
				     sizeof ( rsol ), NDP_OPT_LL_SOURCE ) ) !=0)
		return rc;

	return 0;
}

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
		DBGC ( netdev, "NDP %s neighbour solicitation link-layer "
		       "address option too short at %zd bytes\n",
		       netdev->name, len );
		return -EINVAL;
	}

	/* Create or update neighbour cache entry */
	if ( ( rc = neighbour_define ( netdev, &ipv6_protocol,
				       &sin6_src->sin6_addr,
				       ll_addr_opt->ll_addr ) ) != 0 ) {
		DBGC ( netdev, "NDP %s could not define %s => %s: %s\n",
		       netdev->name, inet6_ntoa ( &sin6_src->sin6_addr ),
		       ll_protocol->ntoa ( ll_addr_opt->ll_addr ),
		       strerror ( rc ) );
		return rc;
	}

	/* Convert neighbour header to advertisement */
	memset ( neigh, 0, offsetof ( typeof ( *neigh ), target ) );
	neigh->icmp.type = ICMPV6_NEIGHBOUR_ADVERTISEMENT;
	neigh->flags = ( NDP_NEIGHBOUR_SOLICITED | NDP_NEIGHBOUR_OVERRIDE );

	/* Send neighbour advertisement */
	if ( ( rc = ndp_tx_ll_addr ( netdev, NULL, sin6_src, neigh,
				     sizeof ( *neigh ),
				     NDP_OPT_LL_TARGET ) ) != 0 )
		return rc;

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
		DBGC ( netdev, "NDP %s neighbour advertisement link-layer "
		       "address option too short at %zd bytes\n",
		       netdev->name, len );
		return -EINVAL;
	}

	/* Update neighbour cache entry, if any */
	if ( ( rc = neighbour_update ( netdev, &ipv6_protocol, &neigh->target,
				       ll_addr_opt->ll_addr ) ) != 0 ) {
		DBGC ( netdev, "NDP %s could not update %s => %s: %s\n",
		       netdev->name, inet6_ntoa ( &neigh->target ),
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
		DBGC ( netdev, "NDP %s router advertisement link-layer address "
		       "option too short at %zd bytes\n", netdev->name, len );
		return -EINVAL;
	}

	/* Define neighbour cache entry */
	if ( ( rc = neighbour_define ( netdev, &ipv6_protocol,
				       &sin6_src->sin6_addr,
				       ll_addr_opt->ll_addr ) ) != 0 ) {
		DBGC ( netdev, "NDP %s could not define %s => %s: %s\n",
		       netdev->name, inet6_ntoa ( &sin6_src->sin6_addr ),
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

	/* Sanity check */
	if ( sizeof ( *prefix_opt ) > len ) {
		DBGC ( netdev, "NDP %s router advertisement prefix option too "
		       "short at %zd bytes\n", netdev->name, len );
		return -EINVAL;
	}

	DBGC ( netdev, "NDP %s found %sdefault router %s ",
	       netdev->name, ( radv->lifetime ? "" : "non-" ),
	       inet6_ntoa ( &sin6_src->sin6_addr ) );
	DBGC ( netdev, "for %s-link %sautonomous prefix %s/%d\n",
	       ( ( prefix_opt->flags & NDP_PREFIX_ON_LINK ) ? "on" : "off" ),
	       ( ( prefix_opt->flags & NDP_PREFIX_AUTONOMOUS ) ? "" : "non-" ),
	       inet6_ntoa ( &prefix_opt->prefix ), prefix_opt->prefix_len );

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
 * Process received NDP packet options
 *
 * @v netdev		Network device
 * @v sin6_src		Source socket address
 * @v ndp		NDP header
 * @v offset		Offset to NDP options
 * @v len		Length of NDP packet
 * @ret rc		Return status code
 */
static int ndp_rx_options ( struct net_device *netdev,
			    struct sockaddr_in6 *sin6_src,
			    union ndp_header *ndp, size_t offset, size_t len ) {
	union ndp_option *option;
	size_t remaining;
	size_t option_len;
	int rc;

	/* Sanity check */
	if ( len < offset ) {
		DBGC ( netdev, "NDP %s packet too short at %zd bytes (min %zd "
		       "bytes)\n", netdev->name, len, offset );
		return -EINVAL;
	}

	/* Search for option */
	option = ( ( ( void * ) ndp ) + offset );
	remaining = ( len - offset );
	while ( remaining ) {

		/* Sanity check */
		if ( ( remaining < sizeof ( option->header ) ) ||
		     ( option->header.blocks == 0 ) ||
		     ( remaining < ( option->header.blocks *
				     NDP_OPTION_BLKSZ ) ) ) {
			DBGC ( netdev, "NDP %s bad option length:\n",
			       netdev->name );
			DBGC_HDA ( netdev, 0, option, remaining );
			return -EINVAL;
		}
		option_len = ( option->header.blocks * NDP_OPTION_BLKSZ );

		/* Handle option */
		if ( ( rc = ndp_rx_option ( netdev, sin6_src, ndp, option,
					    option_len ) ) != 0 )
			return rc;

		/* Move to next option */
		option = ( ( ( void * ) option ) + option_len );
		remaining -= option_len;
	}

	return 0;
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
	size_t len = iob_len ( iobuf );
	int rc;

	/* Process options */
	if ( ( rc = ndp_rx_options ( netdev, sin6_src, ndp,
				     offsetof ( typeof ( *neigh ), option ),
				     len ) ) != 0 )
		goto err_options;

 err_options:
	free_iob ( iobuf );
	return rc;
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
	struct in6_addr *router = &sin6_src->sin6_addr;
	size_t len = iob_len ( iobuf );
	int rc;

	/* Process options */
	if ( ( rc = ndp_rx_options ( netdev, sin6_src, ndp,
				     offsetof ( typeof ( *radv ), option ),
				     len ) ) != 0 )
		goto err_options;

	/* Pass to IPv6 autoconfiguration */
	if ( ( rc = ipv6conf_rx_router_advertisement ( netdev, router,
						       radv, len ) ) != 0 )
		goto err_ipv6conf;

 err_ipv6conf:
 err_options:
	free_iob ( iobuf );
	return rc;
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

/****************************************************************************
 *
 * NDP settings
 *
 */

/** An NDP prefix settings block */
struct ndp_prefix_settings {
	/** Settings interface */
	struct settings settings;
	/** Name */
	char name[4];
	/** Prefix information option */
	struct ndp_prefix_information_option *prefix;
};

/** An NDP settings block */
struct ndp_settings {
	/** Reference counter */
	struct refcnt refcnt;
	/** Settings interface */
	struct settings settings;
	/** Router address */
	struct in6_addr router;
	/** Router lifetime */
	unsigned int lifetime;
	/** Length of NDP options */
	size_t len;
	/** NDP options */
	union ndp_option options[0];
};

/** NDP settings scope */
static const struct settings_scope ndp_settings_scope;

/**
 * Construct NDP tag
 *
 * @v type		NDP option type
 * @v offset		Starting offset of data
 * @v len		Length of data (or 0 to use all remaining data)
 * @ret tag		NDP tag
 */
#define NDP_TAG( type, offset, len ) \
	( ( (len) << 16 ) | ( (offset) << 8 ) | (type) )

/**
 * Extract NDP tag type
 *
 * @v tag		NDP tag
 * @ret type		NDP option type
 */
#define NDP_TAG_TYPE( tag ) ( ( (tag) >> 0 ) & 0xff )

/**
 * Extract NDP tag offset
 *
 * @v tag		NDP tag
 * @ret offset		Starting offset of data
 */
#define NDP_TAG_OFFSET( tag ) ( ( (tag) >> 8 ) & 0xff )

/**
 * Extract NDP tag length
 *
 * @v tag		NDP tag
 * @ret len		Length of data (or 0 to use all remaining data)
 */
#define NDP_TAG_LEN( tag ) ( ( (tag) >> 16 ) & 0xff )

/**
 * Extract NDP tag instance
 *
 * @v tag		NDP tag
 * @ret instance	Instance
 */
#define NDP_TAG_INSTANCE( tag ) ( ( (tag) >> 24 ) & 0xff )

/**
 * Check applicability of NDP setting
 *
 * @v settings		Settings block
 * @v setting		Setting to fetch
 * @ret applies		Setting applies within this settings block
 */
static int ndp_applies ( struct settings *settings __unused,
			 const struct setting *setting ) {

	return ( setting->scope == &ndp_settings_scope );
}

/**
 * Fetch value of NDP setting
 *
 * @v settings		Settings block
 * @v setting		Setting to fetch
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int ndp_fetch ( struct settings *settings,
		       struct setting *setting,
		       void *data, size_t len ) {
	struct ndp_settings *ndpset =
		container_of ( settings, struct ndp_settings, settings );
	struct net_device *netdev =
		container_of ( settings->parent, struct net_device,
			       settings.settings );
	union ndp_option *option;
	unsigned int tag_type;
	unsigned int tag_offset;
	unsigned int tag_len;
	unsigned int tag_instance;
	size_t offset;
	size_t option_len;
	void *option_data;

	/* Parse setting tag */
	tag_type = NDP_TAG_TYPE ( setting->tag );
	tag_offset = NDP_TAG_OFFSET ( setting->tag );
	tag_len = NDP_TAG_LEN ( setting->tag );
	tag_instance = NDP_TAG_INSTANCE ( setting->tag );

	/* Scan through NDP options for requested type.  We can assume
	 * that the options are well-formed, otherwise they would have
	 * been rejected prior to being stored.
	 */
	for ( offset = 0 ; offset < ndpset->len ; offset += option_len ) {

		/* Calculate option length */
		option = ( ( ( void * ) ndpset->options ) + offset );
		option_len = ( option->header.blocks * NDP_OPTION_BLKSZ );

		/* Skip options that do not match this tag */
		if ( option->header.type != tag_type )
			continue;

		/* Skip previous instances of this option */
		if ( tag_instance-- != 0 )
			continue;

		/* Sanity check */
		if ( ( tag_offset + tag_len ) > option_len ) {
			DBGC ( netdev, "NDP %s option %d too short\n",
			       netdev->name, tag_type );
			return -EINVAL;
		}
		if ( ! tag_len )
			tag_len = ( option_len - tag_offset );
		option_data = ( ( ( void * ) option ) + tag_offset );

		/* Copy data to output buffer */
		if ( len > tag_len )
			len = tag_len;
		memcpy ( data, option_data, len );

		/* Default to hex if no type is specified */
		if ( ! setting->type )
			setting->type = &setting_type_hex;

		return tag_len;
	}

	return -ENOENT;
}

/** NDP settings operations */
static struct settings_operations ndp_settings_operations = {
	.applies = ndp_applies,
	.fetch = ndp_fetch,
};

/**
 * Check applicability of NDP per-prefix setting
 *
 * @v settings		Settings block
 * @v setting		Setting to fetch
 * @ret applies		Setting applies within this settings block
 */
static int ndp_prefix_applies ( struct settings *settings __unused,
				const struct setting *setting ) {

	return ( setting->scope == &ipv6_settings_scope );
}

/**
 * Fetch value of NDP IPv6 address setting
 *
 * @v settings		Settings block
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int ndp_prefix_fetch_ip6 ( struct settings *settings, void *data,
				  size_t len ) {
	struct ndp_prefix_settings *prefset =
		container_of ( settings, struct ndp_prefix_settings, settings );
	struct ndp_settings *ndpset =
		container_of ( settings->parent, struct ndp_settings, settings);
	struct net_device *netdev =
		container_of ( ndpset->settings.parent, struct net_device,
			       settings.settings );
	struct ndp_prefix_information_option *prefix = prefset->prefix;
	struct in6_addr ip6;
	int prefix_len;

	/* Skip dead prefixes */
	if ( ! prefix->valid )
		return -ENOENT;

	/* Construct IPv6 address via SLAAC, if applicable */
	memcpy ( &ip6, &prefix->prefix, sizeof ( ip6 ) );
	if ( prefix->flags & NDP_PREFIX_AUTONOMOUS ) {
		prefix_len = ipv6_eui64 ( &ip6, netdev );
		if ( prefix_len < 0 )
			return prefix_len;
		if ( prefix_len != prefix->prefix_len )
			return -EINVAL;
	}

	/* Fill in IPv6 address */
	if ( len > sizeof ( ip6 ) )
		len = sizeof ( ip6 );
	memcpy ( data, &ip6, len );

	return sizeof ( ip6 );
}

/**
 * Fetch value of NDP prefix length setting
 *
 * @v settings		Settings block
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int ndp_prefix_fetch_len6 ( struct settings *settings, void *data,
				   size_t len ) {
	struct ndp_prefix_settings *prefset =
		container_of ( settings, struct ndp_prefix_settings, settings );
	struct ndp_prefix_information_option *prefix = prefset->prefix;
	uint8_t *len6;

	/* Fill in prefix length */
	if ( len >= sizeof ( *len6 ) ) {
		/* We treat an off-link prefix as having a prefix
		 * length covering the entire IPv6 address.
		 */
		len6 = data;
		*len6 = ( ( prefix->flags & NDP_PREFIX_ON_LINK ) ?
			  prefix->prefix_len : -1UL );
	}

	return sizeof ( *len6 );
}

/**
 * Fetch value of NDP router address setting
 *
 * @v settings		Settings block
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int ndp_prefix_fetch_gateway6 ( struct settings *settings,
				       void *data, size_t len ) {
	struct ndp_settings *ndpset =
		container_of ( settings->parent, struct ndp_settings, settings);

	/* Treat non-routing router as non-existent */
	if ( ! ndpset->lifetime )
		return -ENOENT;

	/* Fill in router address */
	if ( len > sizeof ( ndpset->router ) )
		len = sizeof ( ndpset->router );
	memcpy ( data, &ndpset->router, len );

	return sizeof ( ndpset->router );
}

/** An NDP per-prefix setting operation */
struct ndp_prefix_operation {
	/** Generic setting */
	const struct setting *setting;
	/**
	 * Fetch value of setting
	 *
	 * @v settings		Settings block
	 * @v data		Buffer to fill with setting data
	 * @v len		Length of buffer
	 * @ret len		Length of setting data, or negative error
	 */
	int ( * fetch ) ( struct settings *settings, void *data, size_t len );
};

/** NDP per-prefix settings operations */
static struct ndp_prefix_operation ndp_prefix_operations[] = {
	{ &ip6_setting, ndp_prefix_fetch_ip6 },
	{ &len6_setting, ndp_prefix_fetch_len6 },
	{ &gateway6_setting, ndp_prefix_fetch_gateway6 },
};

/**
 * Fetch value of NDP pre-prefix setting
 *
 * @v settings		Settings block
 * @v setting		Setting to fetch
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int ndp_prefix_fetch ( struct settings *settings,
			      struct setting *setting,
			      void *data, size_t len ) {
	struct ndp_prefix_operation *op;
	unsigned int i;

	/* Handle per-prefix settings */
	for ( i = 0 ; i < ( sizeof ( ndp_prefix_operations ) /
			    sizeof ( ndp_prefix_operations[0] ) ) ; i++ ) {
		op = &ndp_prefix_operations[i];
		if ( setting_cmp ( setting, op->setting ) == 0 )
			return op->fetch ( settings, data, len );
	}

	return -ENOENT;
}

/** NDP per-prefix settings operations */
static struct settings_operations ndp_prefix_settings_operations = {
	.applies = ndp_prefix_applies,
	.fetch = ndp_prefix_fetch,
};

/**
 * Register NDP settings
 *
 * @v netdev		Network device
 * @v router		Router address
 * @v lifetime		Router lifetime
 * @v options		NDP options
 * @v len		Length of options
 * @ret rc		Return status code
 */
static int ndp_register_settings ( struct net_device *netdev,
				   struct in6_addr *router,
				   unsigned int lifetime,
				   union ndp_option *options, size_t len ) {
	struct settings *parent = netdev_settings ( netdev );
	union ndp_option *option;
	struct ndp_settings *ndpset;
	struct ndp_prefix_settings *prefset;
	size_t offset;
	size_t option_len;
	unsigned int prefixes;
	unsigned int instance;
	int order;
	int rc;

	/* Count number of prefix options.  We can assume that the
	 * options are well-formed, otherwise they would have been
	 * rejected prior to being stored.
	 */
	order = IPV6_ORDER_PREFIX_ONLY;
	for ( prefixes = 0, offset = 0 ; offset < len ; offset += option_len ) {

		/* Skip non-prefix options */
		option = ( ( ( void * ) options ) + offset );
		option_len = ( option->header.blocks * NDP_OPTION_BLKSZ );
		if ( option->header.type != NDP_OPT_PREFIX )
			continue;

		/* Count number of prefixes */
		prefixes++;

		/* Increase overall order if we have SLAAC addresses */
		if ( option->prefix.flags & NDP_PREFIX_AUTONOMOUS )
			order = IPV6_ORDER_SLAAC;
	}

	/* Allocate and initialise structure */
	ndpset = zalloc ( sizeof ( *ndpset ) + len +
			  ( prefixes * sizeof ( *prefset ) ) );
	if ( ! ndpset ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	ref_init ( &ndpset->refcnt, NULL );
	settings_init ( &ndpset->settings, &ndp_settings_operations,
			&ndpset->refcnt, &ndp_settings_scope );
	ndpset->settings.order = order;
	memcpy ( &ndpset->router, router, sizeof ( ndpset->router ) );
	ndpset->lifetime = lifetime;
	ndpset->len = len;
	memcpy ( ndpset->options, options, len );
	prefset = ( ( ( void * ) ndpset->options ) + len );

	/* Register settings */
	if ( ( rc = register_settings ( &ndpset->settings, parent,
					NDP_SETTINGS_NAME ) ) != 0 )
		goto err_register;

	/* Construct and register per-prefix settings */
	for ( instance = 0, offset = 0 ; offset < len ; offset += option_len ) {

		/* Skip non-prefix options */
		option = ( ( ( void * ) ndpset->options ) + offset );
		option_len = ( option->header.blocks * NDP_OPTION_BLKSZ );
		if ( option->header.type != NDP_OPT_PREFIX )
			continue;

		/* Initialise structure */
		settings_init ( &prefset->settings,
				&ndp_prefix_settings_operations,
				&ndpset->refcnt, &ndp_settings_scope );
		prefset->settings.order =
			( ( option->prefix.flags & NDP_PREFIX_AUTONOMOUS ) ?
			  IPV6_ORDER_SLAAC : IPV6_ORDER_PREFIX_ONLY );
		prefset->prefix = &option->prefix;
		snprintf ( prefset->name, sizeof ( prefset->name ), "%d",
			   instance++ );

		/* Register settings */
		if ( ( rc = register_settings ( &prefset->settings,
						&ndpset->settings,
						prefset->name ) ) != 0 )
			goto err_register_prefix;

		/* Move to next per-prefix settings */
		prefset++;
	}
	assert ( instance == prefixes );

	ref_put ( &ndpset->refcnt );
	return 0;

 err_register_prefix:
	unregister_settings ( &ndpset->settings );
 err_register:
	ref_put ( &ndpset->refcnt );
 err_alloc:
	return rc;
}

/** DNS server setting */
const struct setting ndp_dns6_setting __setting ( SETTING_IP6_EXTRA, dns6 ) = {
	.name = "dns6",
	.description = "DNS server",
	.tag = NDP_TAG ( NDP_OPT_RDNSS,
			 offsetof ( struct ndp_rdnss_option, addresses ), 0 ),
	.type = &setting_type_ipv6,
	.scope = &ndp_settings_scope,
};

/** DNS search list setting */
const struct setting ndp_dnssl_setting __setting ( SETTING_IP_EXTRA, dnssl ) = {
	.name = "dnssl",
	.description = "DNS search list",
	.tag = NDP_TAG ( NDP_OPT_DNSSL,
			 offsetof ( struct ndp_dnssl_option, names ), 0 ),
	.type = &setting_type_dnssl,
	.scope = &ndp_settings_scope,
};

/****************************************************************************
 *
 * IPv6 autoconfiguration
 *
 */

/** An IPv6 configurator */
struct ipv6conf {
	/** Reference count */
	struct refcnt refcnt;
	/** List of configurators */
	struct list_head list;

	/** Job control interface */
	struct interface job;
	/** DHCPv6 interface */
	struct interface dhcp;

	/** Network device being configured */
	struct net_device *netdev;

	/** Retransmission timer */
	struct retry_timer timer;
};

/** List of IPv6 configurators */
static LIST_HEAD ( ipv6confs );

/**
 * Free IPv6 configurator
 *
 * @v refcnt		Reference count
 */
static void ipv6conf_free ( struct refcnt *refcnt ) {
	struct ipv6conf *ipv6conf =
		container_of ( refcnt, struct ipv6conf, refcnt );

	netdev_put ( ipv6conf->netdev );
	free ( ipv6conf );
}

/**
 * Identify IPv6 configurator by network device
 *
 * @v netdev		Network device
 * @ret ipv6		IPv6 configurator, or NULL
 */
static struct ipv6conf * ipv6conf_demux ( struct net_device *netdev ) {
	struct ipv6conf *ipv6conf;

	list_for_each_entry ( ipv6conf, &ipv6confs, list ) {
		if ( ipv6conf->netdev == netdev )
			return ipv6conf;
	}
	return NULL;
}

/**
 * Finish IPv6 autoconfiguration
 *
 * @v ipv6		IPv6 configurator
 * @v rc		Reason for finishing
 */
static void ipv6conf_done ( struct ipv6conf *ipv6conf, int rc ) {

	/* Shut down interfaces */
	intf_shutdown ( &ipv6conf->job, rc );
	intf_shutdown ( &ipv6conf->dhcp, rc );

	/* Stop timer */
	stop_timer ( &ipv6conf->timer );

	/* Remove from list and drop list's reference */
	list_del ( &ipv6conf->list );
	ref_put ( &ipv6conf->refcnt );
}

/**
 * Handle IPv6 configurator timer expiry
 *
 * @v timer		Retry timer
 * @v fail		Failure indicator
 */
static void ipv6conf_expired ( struct retry_timer *timer, int fail ) {
	struct ipv6conf *ipv6conf =
		container_of ( timer, struct ipv6conf, timer );

	/* If we have failed, terminate autoconfiguration */
	if ( fail ) {
		ipv6conf_done ( ipv6conf, -ETIMEDOUT );
		return;
	}

	/* Otherwise, transmit router solicitation and restart timer */
	start_timer ( &ipv6conf->timer );
	ndp_tx_router_solicitation ( ipv6conf->netdev );
}

/**
 * Handle router advertisement during IPv6 autoconfiguration
 *
 * @v netdev		Network device
 * @v router		Router address
 * @v radv		Router advertisement
 * @v len		Length of router advertisement
 * @ret rc		Return status code
 *
 * This function assumes that the router advertisement is well-formed,
 * since it must have already passed through option processing.
 */
static int
ipv6conf_rx_router_advertisement ( struct net_device *netdev,
				   struct in6_addr *router,
				   struct ndp_router_advertisement_header *radv,
				   size_t len ) {
	struct ipv6conf *ipv6conf;
	size_t option_len;
	int stateful;
	int rc;

	/* Identify IPv6 configurator, if any */
	ipv6conf = ipv6conf_demux ( netdev );

	/* Do nothing unless IPv6 autoconfiguration is in progress */
	if ( ! ipv6conf )
		return 0;

	/* If this is not the first solicited router advertisement, ignore it */
	if ( ! timer_running ( &ipv6conf->timer ) )
		return 0;

	/* Stop router solicitation timer */
	stop_timer ( &ipv6conf->timer );

	/* Register NDP settings */
	option_len = ( len - offsetof ( typeof ( *radv ), option ) );
	if ( ( rc = ndp_register_settings ( netdev, router,
					    ntohl ( radv->lifetime ),
					    radv->option, option_len ) ) != 0 )
		return rc;

	/* Start DHCPv6 if required */
	if ( radv->flags & ( NDP_ROUTER_MANAGED | NDP_ROUTER_OTHER ) ) {
		stateful = ( radv->flags & NDP_ROUTER_MANAGED );
		if ( ( rc = start_dhcpv6 ( &ipv6conf->dhcp, netdev,
					   stateful ) ) != 0 ) {
			DBGC ( netdev, "NDP %s could not start state%s DHCPv6: "
			       "%s\n", netdev->name,
			       ( stateful ? "ful" : "less" ), strerror ( rc ) );
			ipv6conf_done ( ipv6conf, rc );
			return rc;
		}
		return 0;
	}

	/* Otherwise, terminate autoconfiguration */
	ipv6conf_done ( ipv6conf, 0 );

	return 0;
}

/** IPv6 configurator job interface operations */
static struct interface_operation ipv6conf_job_op[] = {
	INTF_OP ( intf_close, struct ipv6conf *, ipv6conf_done ),
};

/** IPv6 configurator job interface descriptor */
static struct interface_descriptor ipv6conf_job_desc =
	INTF_DESC ( struct ipv6conf, job, ipv6conf_job_op );

/** IPv6 configurator DHCPv6 interface operations */
static struct interface_operation ipv6conf_dhcp_op[] = {
	INTF_OP ( intf_close, struct ipv6conf *, ipv6conf_done ),
};

/** IPv6 configurator DHCPv6 interface descriptor */
static struct interface_descriptor ipv6conf_dhcp_desc =
	INTF_DESC ( struct ipv6conf, dhcp, ipv6conf_dhcp_op );

/**
 * Start IPv6 autoconfiguration
 *
 * @v job		Job control interface
 * @v netdev		Network device
 * @ret rc		Return status code
 */
int start_ipv6conf ( struct interface *job, struct net_device *netdev ) {
	struct ipv6conf *ipv6conf;

	/* Allocate and initialise structure */
	ipv6conf = zalloc ( sizeof ( *ipv6conf ) );
	if ( ! ipv6conf )
		return -ENOMEM;
	ref_init ( &ipv6conf->refcnt, ipv6conf_free );
	intf_init ( &ipv6conf->job, &ipv6conf_job_desc, &ipv6conf->refcnt );
	intf_init ( &ipv6conf->dhcp, &ipv6conf_dhcp_desc, &ipv6conf->refcnt );
	timer_init ( &ipv6conf->timer, ipv6conf_expired, &ipv6conf->refcnt );
	set_timer_limits ( &ipv6conf->timer, IPV6CONF_MIN_TIMEOUT,
			   IPV6CONF_MAX_TIMEOUT );
	ipv6conf->netdev = netdev_get ( netdev );

	/* Start timer to initiate router solicitation */
	start_timer_nodelay ( &ipv6conf->timer );

	/* Attach parent interface, transfer reference to list, and return */
	intf_plug_plug ( &ipv6conf->job, job );
	list_add ( &ipv6conf->list, &ipv6confs );
	return 0;
}

/** IPv6 network device configurator */
struct net_device_configurator ipv6_configurator __net_device_configurator = {
	.name = "ipv6",
	.start = start_ipv6conf,
};
