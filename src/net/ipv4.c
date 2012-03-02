#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/list.h>
#include <ipxe/in.h>
#include <ipxe/arp.h>
#include <ipxe/if_ether.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>
#include <ipxe/ip.h>
#include <ipxe/tcpip.h>
#include <ipxe/dhcp.h>
#include <ipxe/settings.h>
#include <ipxe/timer.h>

/** @file
 *
 * IPv4 protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/* Unique IP datagram identification number (high byte) */
static uint8_t next_ident_high = 0;

/** List of IPv4 miniroutes */
struct list_head ipv4_miniroutes = LIST_HEAD_INIT ( ipv4_miniroutes );

/** List of fragment reassembly buffers */
static LIST_HEAD ( ipv4_fragments );

/** Fragment reassembly timeout */
#define IP_FRAG_TIMEOUT ( TICKS_PER_SEC / 2 )

/**
 * Add IPv4 minirouting table entry
 *
 * @v netdev		Network device
 * @v address		IPv4 address
 * @v netmask		Subnet mask
 * @v gateway		Gateway address (if any)
 * @ret miniroute	Routing table entry, or NULL
 */
static struct ipv4_miniroute * __malloc
add_ipv4_miniroute ( struct net_device *netdev, struct in_addr address,
		     struct in_addr netmask, struct in_addr gateway ) {
	struct ipv4_miniroute *miniroute;

	DBGC ( netdev, "IPv4 add %s", inet_ntoa ( address ) );
	DBGC ( netdev, "/%s ", inet_ntoa ( netmask ) );
	if ( gateway.s_addr )
		DBGC ( netdev, "gw %s ", inet_ntoa ( gateway ) );
	DBGC ( netdev, "via %s\n", netdev->name );

	/* Allocate and populate miniroute structure */
	miniroute = malloc ( sizeof ( *miniroute ) );
	if ( ! miniroute ) {
		DBGC ( netdev, "IPv4 could not add miniroute\n" );
		return NULL;
	}

	/* Record routing information */
	miniroute->netdev = netdev_get ( netdev );
	miniroute->address = address;
	miniroute->netmask = netmask;
	miniroute->gateway = gateway;
		
	/* Add to end of list if we have a gateway, otherwise
	 * to start of list.
	 */
	if ( gateway.s_addr ) {
		list_add_tail ( &miniroute->list, &ipv4_miniroutes );
	} else {
		list_add ( &miniroute->list, &ipv4_miniroutes );
	}

	return miniroute;
}

/**
 * Delete IPv4 minirouting table entry
 *
 * @v miniroute		Routing table entry
 */
static void del_ipv4_miniroute ( struct ipv4_miniroute *miniroute ) {
	struct net_device *netdev = miniroute->netdev;

	DBGC ( netdev, "IPv4 del %s", inet_ntoa ( miniroute->address ) );
	DBGC ( netdev, "/%s ", inet_ntoa ( miniroute->netmask ) );
	if ( miniroute->gateway.s_addr )
		DBGC ( netdev, "gw %s ", inet_ntoa ( miniroute->gateway ) );
	DBGC ( netdev, "via %s\n", miniroute->netdev->name );

	netdev_put ( miniroute->netdev );
	list_del ( &miniroute->list );
	free ( miniroute );
}

/**
 * Perform IPv4 routing
 *
 * @v dest		Final destination address
 * @ret dest		Next hop destination address
 * @ret miniroute	Routing table entry to use, or NULL if no route
 *
 * If the route requires use of a gateway, the next hop destination
 * address will be overwritten with the gateway address.
 */
static struct ipv4_miniroute * ipv4_route ( struct in_addr *dest ) {
	struct ipv4_miniroute *miniroute;
	int local;
	int has_gw;

	/* Find first usable route in routing table */
	list_for_each_entry ( miniroute, &ipv4_miniroutes, list ) {
		if ( ! netdev_is_open ( miniroute->netdev ) )
			continue;
		local = ( ( ( dest->s_addr ^ miniroute->address.s_addr )
			    & miniroute->netmask.s_addr ) == 0 );
		has_gw = ( miniroute->gateway.s_addr );
		if ( local || has_gw ) {
			if ( ! local )
				*dest = miniroute->gateway;
			return miniroute;
		}
	}

	return NULL;
}

/**
 * Expire fragment reassembly buffer
 *
 * @v timer		Retry timer
 * @v fail		Failure indicator
 */
static void ipv4_fragment_expired ( struct retry_timer *timer,
				    int fail __unused ) {
	struct ipv4_fragment *frag =
		container_of ( timer, struct ipv4_fragment, timer );
	struct iphdr *iphdr = frag->iobuf->data;

	DBGC ( iphdr->src, "IPv4 fragment %04x expired\n",
	       ntohs ( iphdr->ident ) );
	free_iob ( frag->iobuf );
	list_del ( &frag->list );
	free ( frag );
}

/**
 * Find matching fragment reassembly buffer
 *
 * @v iphdr		IPv4 header
 * @ret frag		Fragment reassembly buffer, or NULL
 */
static struct ipv4_fragment * ipv4_fragment ( struct iphdr *iphdr ) {
	struct ipv4_fragment *frag;
	struct iphdr *frag_iphdr;

	list_for_each_entry ( frag, &ipv4_fragments, list ) {
		frag_iphdr = frag->iobuf->data;

		if ( ( iphdr->src.s_addr == frag_iphdr->src.s_addr ) &&
		     ( iphdr->ident == frag_iphdr->ident ) ) {
			return frag;
		}
	}

	return NULL;
}

/**
 * Fragment reassembler
 *
 * @v iobuf		I/O buffer
 * @ret iobuf		Reassembled packet, or NULL
 */
static struct io_buffer * ipv4_reassemble ( struct io_buffer *iobuf ) {
	struct iphdr *iphdr = iobuf->data;
	size_t offset = ( ( ntohs ( iphdr->frags ) & IP_MASK_OFFSET ) << 3 );
	unsigned int more_frags = ( iphdr->frags & htons ( IP_MASK_MOREFRAGS ));
	size_t hdrlen = ( ( iphdr->verhdrlen & IP_MASK_HLEN ) * 4 );
	struct ipv4_fragment *frag;
	size_t expected_offset;
	struct io_buffer *new_iobuf;

	/* Find matching fragment reassembly buffer, if any */
	frag = ipv4_fragment ( iphdr );

	/* Drop out-of-order fragments */
	expected_offset = ( frag ? frag->offset : 0 );
	if ( offset != expected_offset ) {
		DBGC ( iphdr->src, "IPv4 dropping out-of-sequence fragment "
		       "%04x (%zd+%zd, expected %zd)\n",
		       ntohs ( iphdr->ident ), offset,
		      ( iob_len ( iobuf ) - hdrlen ), expected_offset );
		goto drop;
	}

	/* Create or extend fragment reassembly buffer as applicable */
	if ( frag == NULL ) {

		/* Create new fragment reassembly buffer */
		frag = zalloc ( sizeof ( *frag ) );
		if ( ! frag )
			goto drop;
		list_add ( &frag->list, &ipv4_fragments );
		frag->iobuf = iobuf;
		frag->offset = ( iob_len ( iobuf ) - hdrlen );
		timer_init ( &frag->timer, ipv4_fragment_expired, NULL );

	} else {

		/* Extend reassembly buffer */
		iob_pull ( iobuf, hdrlen );
		new_iobuf = alloc_iob ( iob_len ( frag->iobuf ) +
					iob_len ( iobuf ) );
		if ( ! new_iobuf ) {
			DBGC ( iphdr->src, "IPv4 could not extend reassembly "
			       "buffer to %zd bytes\n",
			       iob_len ( frag->iobuf ) + iob_len ( iobuf ) );
			goto drop;
		}
		memcpy ( iob_put ( new_iobuf, iob_len ( frag->iobuf ) ),
			 frag->iobuf->data, iob_len ( frag->iobuf ) );
		memcpy ( iob_put ( new_iobuf, iob_len ( iobuf ) ),
			 iobuf->data, iob_len ( iobuf ) );
		free_iob ( frag->iobuf );
		frag->iobuf = new_iobuf;
		frag->offset += iob_len ( iobuf );
		free_iob ( iobuf );
		iphdr = frag->iobuf->data;
		iphdr->len = ntohs ( iob_len ( frag->iobuf ) );

		/* Stop fragment reassembly timer */
		stop_timer ( &frag->timer );

		/* If this is the final fragment, return it */
		if ( ! more_frags ) {
			iobuf = frag->iobuf;
			list_del ( &frag->list );
			free ( frag );
			return iobuf;
		}
	}

	/* (Re)start fragment reassembly timer */
	start_timer_fixed ( &frag->timer, IP_FRAG_TIMEOUT );

	return NULL;

 drop:
	free_iob ( iobuf );
	return NULL;
}

/**
 * Add IPv4 pseudo-header checksum to existing checksum
 *
 * @v iobuf		I/O buffer
 * @v csum		Existing checksum
 * @ret csum		Updated checksum
 */
static uint16_t ipv4_pshdr_chksum ( struct io_buffer *iobuf, uint16_t csum ) {
	struct ipv4_pseudo_header pshdr;
	struct iphdr *iphdr = iobuf->data;
	size_t hdrlen = ( ( iphdr->verhdrlen & IP_MASK_HLEN ) * 4 );

	/* Build pseudo-header */
	pshdr.src = iphdr->src;
	pshdr.dest = iphdr->dest;
	pshdr.zero_padding = 0x00;
	pshdr.protocol = iphdr->protocol;
	pshdr.len = htons ( iob_len ( iobuf ) - hdrlen );

	/* Update the checksum value */
	return tcpip_continue_chksum ( csum, &pshdr, sizeof ( pshdr ) );
}

/**
 * Transmit IP packet
 *
 * @v iobuf		I/O buffer
 * @v tcpip		Transport-layer protocol
 * @v st_src		Source network-layer address
 * @v st_dest		Destination network-layer address
 * @v netdev		Network device to use if no route found, or NULL
 * @v trans_csum	Transport-layer checksum to complete, or NULL
 * @ret rc		Status
 *
 * This function expects a transport-layer segment and prepends the IP header
 */
static int ipv4_tx ( struct io_buffer *iobuf,
		     struct tcpip_protocol *tcpip_protocol,
		     struct sockaddr_tcpip *st_src,
		     struct sockaddr_tcpip *st_dest,
		     struct net_device *netdev,
		     uint16_t *trans_csum ) {
	struct iphdr *iphdr = iob_push ( iobuf, sizeof ( *iphdr ) );
	struct sockaddr_in *sin_src = ( ( struct sockaddr_in * ) st_src );
	struct sockaddr_in *sin_dest = ( ( struct sockaddr_in * ) st_dest );
	struct ipv4_miniroute *miniroute;
	struct in_addr next_hop;
	struct in_addr netmask = { .s_addr = 0 };
	uint8_t ll_dest_buf[MAX_LL_ADDR_LEN];
	const void *ll_dest;
	int rc;

	/* Fill up the IP header, except source address */
	memset ( iphdr, 0, sizeof ( *iphdr ) );
	iphdr->verhdrlen = ( IP_VER | ( sizeof ( *iphdr ) / 4 ) );
	iphdr->service = IP_TOS;
	iphdr->len = htons ( iob_len ( iobuf ) );	
	iphdr->ttl = IP_TTL;
	iphdr->protocol = tcpip_protocol->tcpip_proto;
	iphdr->dest = sin_dest->sin_addr;

	/* Use routing table to identify next hop and transmitting netdev */
	next_hop = iphdr->dest;
	if ( sin_src )
		iphdr->src = sin_src->sin_addr;
	if ( ( next_hop.s_addr != INADDR_BROADCAST ) &&
	     ( ! IN_MULTICAST ( ntohl ( next_hop.s_addr ) ) ) &&
	     ( ( miniroute = ipv4_route ( &next_hop ) ) != NULL ) ) {
		iphdr->src = miniroute->address;
		netmask = miniroute->netmask;
		netdev = miniroute->netdev;
	}
	if ( ! netdev ) {
		DBGC ( sin_dest->sin_addr, "IPv4 has no route to %s\n",
		       inet_ntoa ( iphdr->dest ) );
		rc = -ENETUNREACH;
		goto err;
	}

	/* (Ab)use the "ident" field to convey metadata about the
	 * network device statistics into packet traces.  Useful for
	 * extracting debug information from non-debug builds.
	 */
	iphdr->ident = htons ( ( (++next_ident_high) << 8 ) |
			       ( ( netdev->rx_stats.bad & 0xf ) << 4 ) |
			       ( ( netdev->rx_stats.good & 0xf ) << 0 ) );

	/* Fix up checksums */
	if ( trans_csum )
		*trans_csum = ipv4_pshdr_chksum ( iobuf, *trans_csum );
	iphdr->chksum = tcpip_chksum ( iphdr, sizeof ( *iphdr ) );

	/* Print IP4 header for debugging */
	DBGC2 ( sin_dest->sin_addr, "IPv4 TX %s->", inet_ntoa ( iphdr->src ) );
	DBGC2 ( sin_dest->sin_addr, "%s len %d proto %d id %04x csum %04x\n",
		inet_ntoa ( iphdr->dest ), ntohs ( iphdr->len ),
		iphdr->protocol, ntohs ( iphdr->ident ),
		ntohs ( iphdr->chksum ) );

	/* Calculate link-layer destination address, if possible */
	if ( ( ( next_hop.s_addr ^ INADDR_BROADCAST ) & ~netmask.s_addr ) == 0){
		/* Broadcast address */
		ll_dest = netdev->ll_broadcast;
	} else if ( IN_MULTICAST ( ntohl ( next_hop.s_addr ) ) ) {
		/* Multicast address */
		if ( ( rc = netdev->ll_protocol->mc_hash ( AF_INET, &next_hop,
							   ll_dest_buf ) ) !=0){
			DBGC ( sin_dest->sin_addr, "IPv4 could not hash "
			       "multicast %s: %s\n",
			       inet_ntoa ( next_hop ), strerror ( rc ) );
			return rc;
		}
		ll_dest = ll_dest_buf;
	} else {
		/* Unicast address */
		ll_dest = NULL;
	}

	/* Hand off to link layer (via ARP if applicable) */
	if ( ll_dest ) {
		if ( ( rc = net_tx ( iobuf, netdev, &ipv4_protocol, ll_dest,
				     netdev->ll_addr ) ) != 0 ) {
			DBGC ( sin_dest->sin_addr, "IPv4 could not transmit "
			       "packet via %s: %s\n",
			       netdev->name, strerror ( rc ) );
			return rc;
		}
	} else {
		if ( ( rc = arp_tx ( iobuf, netdev, &ipv4_protocol, &next_hop,
				     &iphdr->src, netdev->ll_addr ) ) != 0 ) {
			DBGC ( sin_dest->sin_addr, "IPv4 could not transmit "
			       "packet via %s: %s\n",
			       netdev->name, strerror ( rc ) );
			return rc;
		}
	}

	return 0;

 err:
	free_iob ( iobuf );
	return rc;
}

/**
 * Check if network device has any IPv4 address
 *
 * @v netdev		Network device
 * @ret has_any_addr	Network device has any IPv4 address
 */
static int ipv4_has_any_addr ( struct net_device *netdev ) {
	struct ipv4_miniroute *miniroute;

	list_for_each_entry ( miniroute, &ipv4_miniroutes, list ) {
		if ( miniroute->netdev == netdev )
			return 1;
	}
	return 0;
}

/**
 * Check if network device has a specific IPv4 address
 *
 * @v netdev		Network device
 * @v addr		IPv4 address
 * @ret has_addr	Network device has this IPv4 address
 */
static int ipv4_has_addr ( struct net_device *netdev, struct in_addr addr ) {
	struct ipv4_miniroute *miniroute;

	list_for_each_entry ( miniroute, &ipv4_miniroutes, list ) {
		if ( ( miniroute->netdev == netdev ) &&
		     ( miniroute->address.s_addr == addr.s_addr ) ) {
			/* Found matching address */
			return 1;
		}
	}
	return 0;
}

/**
 * Process incoming packets
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v ll_dest		Link-layer destination address
 * @v ll_source		Link-layer destination source
 * @v flags		Packet flags
 * @ret rc		Return status code
 *
 * This function expects an IP4 network datagram. It processes the headers 
 * and sends it to the transport layer.
 */
static int ipv4_rx ( struct io_buffer *iobuf,
		     struct net_device *netdev,
		     const void *ll_dest __unused,
		     const void *ll_source __unused,
		     unsigned int flags ) {
	struct iphdr *iphdr = iobuf->data;
	size_t hdrlen;
	size_t len;
	union {
		struct sockaddr_in sin;
		struct sockaddr_tcpip st;
	} src, dest;
	uint16_t csum;
	uint16_t pshdr_csum;
	int rc;

	/* Sanity check the IPv4 header */
	if ( iob_len ( iobuf ) < sizeof ( *iphdr ) ) {
		DBGC ( iphdr->src, "IPv4 packet too short at %zd bytes (min "
		       "%zd bytes)\n", iob_len ( iobuf ), sizeof ( *iphdr ) );
		goto err;
	}
	if ( ( iphdr->verhdrlen & IP_MASK_VER ) != IP_VER ) {
		DBGC ( iphdr->src, "IPv4 version %#02x not supported\n",
		       iphdr->verhdrlen );
		goto err;
	}
	hdrlen = ( ( iphdr->verhdrlen & IP_MASK_HLEN ) * 4 );
	if ( hdrlen < sizeof ( *iphdr ) ) {
		DBGC ( iphdr->src, "IPv4 header too short at %zd bytes (min "
		       "%zd bytes)\n", hdrlen, sizeof ( *iphdr ) );
		goto err;
	}
	if ( hdrlen > iob_len ( iobuf ) ) {
		DBGC ( iphdr->src, "IPv4 header too long at %zd bytes "
		       "(packet is %zd bytes)\n", hdrlen, iob_len ( iobuf ) );
		goto err;
	}
	if ( ( csum = tcpip_chksum ( iphdr, hdrlen ) ) != 0 ) {
		DBGC ( iphdr->src, "IPv4 checksum incorrect (is %04x "
		       "including checksum field, should be 0000)\n", csum );
		goto err;
	}
	len = ntohs ( iphdr->len );
	if ( len < hdrlen ) {
		DBGC ( iphdr->src, "IPv4 length too short at %zd bytes "
		       "(header is %zd bytes)\n", len, hdrlen );
		goto err;
	}
	if ( len > iob_len ( iobuf ) ) {
		DBGC ( iphdr->src, "IPv4 length too long at %zd bytes "
		       "(packet is %zd bytes)\n", len, iob_len ( iobuf ) );
		goto err;
	}

	/* Truncate packet to correct length */
	iob_unput ( iobuf, ( iob_len ( iobuf ) - len ) );

	/* Print IPv4 header for debugging */
	DBGC2 ( iphdr->src, "IPv4 RX %s<-", inet_ntoa ( iphdr->dest ) );
	DBGC2 ( iphdr->src, "%s len %d proto %d id %04x csum %04x\n",
		inet_ntoa ( iphdr->src ), ntohs ( iphdr->len ), iphdr->protocol,
		ntohs ( iphdr->ident ), ntohs ( iphdr->chksum ) );

	/* Discard unicast packets not destined for us */
	if ( ( ! ( flags & LL_MULTICAST ) ) &&
	     ipv4_has_any_addr ( netdev ) &&
	     ( ! ipv4_has_addr ( netdev, iphdr->dest ) ) ) {
		DBGC ( iphdr->src, "IPv4 discarding non-local unicast packet "
		       "for %s\n", inet_ntoa ( iphdr->dest ) );
		goto err;
	}

	/* Perform fragment reassembly if applicable */
	if ( iphdr->frags & htons ( IP_MASK_OFFSET | IP_MASK_MOREFRAGS ) ) {
		/* Pass the fragment to ipv4_reassemble() which returns
		 * either a fully reassembled I/O buffer or NULL.
		 */
		iobuf = ipv4_reassemble ( iobuf );
		if ( ! iobuf )
			return 0;
		iphdr = iobuf->data;
		hdrlen = ( ( iphdr->verhdrlen & IP_MASK_HLEN ) * 4 );
	}

	/* Construct socket addresses, calculate pseudo-header
	 * checksum, and hand off to transport layer
	 */
	memset ( &src, 0, sizeof ( src ) );
	src.sin.sin_family = AF_INET;
	src.sin.sin_addr = iphdr->src;
	memset ( &dest, 0, sizeof ( dest ) );
	dest.sin.sin_family = AF_INET;
	dest.sin.sin_addr = iphdr->dest;
	pshdr_csum = ipv4_pshdr_chksum ( iobuf, TCPIP_EMPTY_CSUM );
	iob_pull ( iobuf, hdrlen );
	if ( ( rc = tcpip_rx ( iobuf, iphdr->protocol, &src.st,
			       &dest.st, pshdr_csum ) ) != 0 ) {
		DBGC ( src.sin.sin_addr, "IPv4 received packet rejected by "
		       "stack: %s\n", strerror ( rc ) );
		return rc;
	}

	return 0;

 err:
	free_iob ( iobuf );
	return -EINVAL;
}

/** 
 * Check existence of IPv4 address for ARP
 *
 * @v netdev		Network device
 * @v net_addr		Network-layer address
 * @ret rc		Return status code
 */
static int ipv4_arp_check ( struct net_device *netdev, const void *net_addr ) {
	const struct in_addr *address = net_addr;

	if ( ipv4_has_addr ( netdev, *address ) )
		return 0;

	return -ENOENT;
}

/**
 * Convert IPv4 address to dotted-quad notation
 *
 * @v in	IP address
 * @ret string	IP address in dotted-quad notation
 */
char * inet_ntoa ( struct in_addr in ) {
	static char buf[16]; /* "xxx.xxx.xxx.xxx" */
	uint8_t *bytes = ( uint8_t * ) &in;
	
	sprintf ( buf, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3] );
	return buf;
}

/**
 * Transcribe IP address
 *
 * @v net_addr	IP address
 * @ret string	IP address in dotted-quad notation
 *
 */
static const char * ipv4_ntoa ( const void *net_addr ) {
	return inet_ntoa ( * ( ( struct in_addr * ) net_addr ) );
}

/** IPv4 protocol */
struct net_protocol ipv4_protocol __net_protocol = {
	.name = "IP",
	.net_proto = htons ( ETH_P_IP ),
	.net_addr_len = sizeof ( struct in_addr ),
	.rx = ipv4_rx,
	.ntoa = ipv4_ntoa,
};

/** IPv4 TCPIP net protocol */
struct tcpip_net_protocol ipv4_tcpip_protocol __tcpip_net_protocol = {
	.name = "IPv4",
	.sa_family = AF_INET,
	.tx = ipv4_tx,
};

/** IPv4 ARP protocol */
struct arp_net_protocol ipv4_arp_protocol __arp_net_protocol = {
	.net_protocol = &ipv4_protocol,
	.check = ipv4_arp_check,
};

/******************************************************************************
 *
 * Settings
 *
 ******************************************************************************
 */

/** IPv4 address setting */
struct setting ip_setting __setting ( SETTING_IPv4 ) = {
	.name = "ip",
	.description = "IP address",
	.tag = DHCP_EB_YIADDR,
	.type = &setting_type_ipv4,
};

/** IPv4 subnet mask setting */
struct setting netmask_setting __setting ( SETTING_IPv4 ) = {
	.name = "netmask",
	.description = "Subnet mask",
	.tag = DHCP_SUBNET_MASK,
	.type = &setting_type_ipv4,
};

/** Default gateway setting */
struct setting gateway_setting __setting ( SETTING_IPv4 ) = {
	.name = "gateway",
	.description = "Default gateway",
	.tag = DHCP_ROUTERS,
	.type = &setting_type_ipv4,
};

/**
 * Create IPv4 routing table based on configured settings
 *
 * @ret rc		Return status code
 */
static int ipv4_create_routes ( void ) {
	struct ipv4_miniroute *miniroute;
	struct ipv4_miniroute *tmp;
	struct net_device *netdev;
	struct settings *settings;
	struct in_addr address = { 0 };
	struct in_addr netmask = { 0 };
	struct in_addr gateway = { 0 };

	/* Delete all existing routes */
	list_for_each_entry_safe ( miniroute, tmp, &ipv4_miniroutes, list )
		del_ipv4_miniroute ( miniroute );

	/* Create a route for each configured network device */
	for_each_netdev ( netdev ) {
		settings = netdev_settings ( netdev );
		/* Get IPv4 address */
		address.s_addr = 0;
		fetch_ipv4_setting ( settings, &ip_setting, &address );
		if ( ! address.s_addr )
			continue;
		/* Get subnet mask */
		fetch_ipv4_setting ( settings, &netmask_setting, &netmask );
		/* Calculate default netmask, if necessary */
		if ( ! netmask.s_addr ) {
			if ( IN_CLASSA ( ntohl ( address.s_addr ) ) ) {
				netmask.s_addr = htonl ( IN_CLASSA_NET );
			} else if ( IN_CLASSB ( ntohl ( address.s_addr ) ) ) {
				netmask.s_addr = htonl ( IN_CLASSB_NET );
			} else if ( IN_CLASSC ( ntohl ( address.s_addr ) ) ) {
				netmask.s_addr = htonl ( IN_CLASSC_NET );
			}
		}
		/* Get default gateway, if present */
		fetch_ipv4_setting ( settings, &gateway_setting, &gateway );
		/* Configure route */
		miniroute = add_ipv4_miniroute ( netdev, address,
						 netmask, gateway );
		if ( ! miniroute )
			return -ENOMEM;
	}

	return 0;
}

/** IPv4 settings applicator */
struct settings_applicator ipv4_settings_applicator __settings_applicator = {
	.apply = ipv4_create_routes,
};

/* Drag in ICMP */
REQUIRE_OBJECT ( icmp );
