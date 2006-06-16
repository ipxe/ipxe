#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <byteswap.h>
#include <malloc.h>
#include <vsprintf.h>
#include <gpxe/list.h>
#include <gpxe/in.h>
#include <gpxe/arp.h>

#include <ip.h>


#include <gpxe/if_ether.h>
#include <gpxe/pkbuff.h>
#include <gpxe/netdevice.h>
#include "uip/uip.h"
#include <gpxe/ip.h>

/** @file
 *
 * IPv4 protocol
 *
 * The gPXE IP stack is currently implemented on top of the uIP
 * protocol stack.  This file provides wrappers around uIP so that
 * higher-level protocol implementations do not need to talk directly
 * to uIP (which has a somewhat baroque API).
 *
 */

struct net_protocol ipv4_protocol;

/** An IPv4 address/routing table entry */
struct ipv4_miniroute {
	/** List of miniroutes */
	struct list_head list;
	/** Network device */
	struct net_device *netdev;
	/** IPv4 address */
	struct in_addr address;
	/** Subnet mask */
	struct in_addr netmask;
	/** Gateway address */
	struct in_addr gateway;
};

/** List of IPv4 miniroutes */
static LIST_HEAD ( miniroutes );

/**
 * Add IPv4 interface
 *
 * @v netdev	Network device
 * @v address	IPv4 address
 * @v netmask	Subnet mask
 * @v gateway	Gateway address (or @c INADDR_NONE for no gateway)
 * @ret rc	Return status code
 *
 */
int add_ipv4_address ( struct net_device *netdev, struct in_addr address,
		       struct in_addr netmask, struct in_addr gateway ) {
	struct ipv4_miniroute *miniroute;

	/* Allocate and populate miniroute structure */
	miniroute = malloc ( sizeof ( *miniroute ) );
	if ( ! miniroute )
		return -ENOMEM;
	miniroute->netdev = netdev;
	miniroute->address = address;
	miniroute->netmask = netmask;
	miniroute->gateway = gateway;
	
	/* Add to end of list if we have a gateway, otherwise to start
	 * of list.
	 */
	if ( gateway.s_addr != INADDR_NONE ) {
		list_add_tail ( &miniroute->list, &miniroutes );
	} else {
		list_add ( &miniroute->list, &miniroutes );
	}
	return 0;
}

/**
 * Remove IPv4 interface
 *
 * @v netdev	Network device
 */
void del_ipv4_address ( struct net_device *netdev ) {
	struct ipv4_miniroute *miniroute;

	list_for_each_entry ( miniroute, &miniroutes, list ) {
		if ( miniroute->netdev == netdev ) {
			list_del ( &miniroute->list );
			break;
		}
	}
}

/**
 * Transmit packet constructed by uIP
 *
 * @v pkb		Packet buffer
 * @ret rc		Return status code
 *
 */
int ipv4_uip_transmit ( struct pk_buff *pkb ) {
	struct iphdr *iphdr = pkb->data;
	struct ipv4_miniroute *miniroute;
	struct net_device *netdev = NULL;
	struct in_addr next_hop;
	struct in_addr source;
	uint8_t ll_dest_buf[MAX_LL_ADDR_LEN];
	const uint8_t *ll_dest = ll_dest_buf;
	int rc;

	/* Use routing table to identify next hop and transmitting netdev */
	next_hop = iphdr->dest;
	list_for_each_entry ( miniroute, &miniroutes, list ) {
		if ( ( ( ( iphdr->dest.s_addr ^ miniroute->address.s_addr ) &
			 miniroute->netmask.s_addr ) == 0 ) ||
		     ( miniroute->gateway.s_addr != INADDR_NONE ) ) {
			netdev = miniroute->netdev;
			source = miniroute->address;
			if ( miniroute->gateway.s_addr != INADDR_NONE )
				next_hop = miniroute->gateway;
			break;
		}
	}

	/* Abort if no network device identified */
	if ( ! netdev ) {
		DBG ( "No route to %s\n", inet_ntoa ( iphdr->dest ) );
		rc = -EHOSTUNREACH;
		goto err;
	}

	/* Determine link-layer destination address */
	if ( next_hop.s_addr == INADDR_BROADCAST ) {
		/* Broadcast address */
		ll_dest = netdev->ll_protocol->ll_broadcast;
	} else if ( IN_MULTICAST ( next_hop.s_addr ) ) {
		/* Special case: IPv4 multicast over Ethernet.  This
		 * code may need to be generalised once we find out
		 * what happens for other link layers.
		 */
		uint8_t *next_hop_bytes = ( uint8_t * ) &next_hop;
		ll_dest_buf[0] = 0x01;
		ll_dest_buf[0] = 0x00;
		ll_dest_buf[0] = 0x5e;
		ll_dest_buf[3] = next_hop_bytes[1] & 0x7f;
		ll_dest_buf[4] = next_hop_bytes[2];
		ll_dest_buf[5] = next_hop_bytes[3];
	} else {
		/* Unicast address: resolve via ARP */
		if ( ( rc = arp_resolve ( netdev, &ipv4_protocol, &next_hop,
					  &source, ll_dest_buf ) ) != 0 ) {
			DBG ( "No ARP entry for %s\n",
			      inet_ntoa ( iphdr->dest ) );
			goto err;
		}
	}
	
	/* Hand off to link layer */
	return net_transmit ( pkb, netdev, &ipv4_protocol, ll_dest );

 err:
	free_pkb ( pkb );
	return rc;
}

/**
 * Process incoming IP packets
 *
 * @v pkb		Packet buffer
 * @ret rc		Return status code
 *
 * This handles IP packets by handing them off to the uIP protocol
 * stack.
 */
static int ipv4_rx ( struct pk_buff *pkb ) {

	/* Transfer to uIP buffer.  Horrendously space-inefficient,
	 * but will do as a proof-of-concept for now.
	 */
	uip_len = pkb_len ( pkb );
	memcpy ( uip_buf, pkb->data, uip_len );
	free_pkb ( pkb );

	/* Hand to uIP for processing */
	uip_input ();
	if ( uip_len > 0 ) {
		pkb = alloc_pkb ( MAX_LL_HEADER_LEN + uip_len );
		if ( ! pkb )
			return -ENOMEM;
		pkb_reserve ( pkb, MAX_LL_HEADER_LEN );
		memcpy ( pkb_put ( pkb, uip_len ), uip_buf, uip_len );
		ipv4_uip_transmit ( pkb );
	}
	return 0;
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
struct net_protocol ipv4_protocol = {
	.name = "IP",
	.net_proto = htons ( ETH_P_IP ),
	.net_addr_len = sizeof ( struct in_addr ),
	.rx_process = ipv4_rx,
	.ntoa = ipv4_ntoa,
};

NET_PROTOCOL ( ipv4_protocol );

/** IPv4 address for the static single net device */
struct net_address static_single_ipv4_address = {
	.net_protocol = &ipv4_protocol,

#warning "Remove this static-IP hack"
	.net_addr = { 0x0a, 0xfe, 0xfe, 0x01 },
};

STATIC_SINGLE_NETDEV_ADDRESS ( static_single_ipv4_address );
