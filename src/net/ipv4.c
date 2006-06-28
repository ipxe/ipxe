#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <byteswap.h>
#include <malloc.h>
#include <vsprintf.h>
#include <gpxe/list.h>
#include <gpxe/in.h>
#include <gpxe/arp.h>
#include <gpxe/if_ether.h>
#include <gpxe/pkbuff.h>
#include <gpxe/netdevice.h>
#include "uip/uip.h"
#include <gpxe/ip.h>
#include <gpxe/interface.h>

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

/* Unique IP datagram identification number */
static uint16_t next_ident = 0;

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
 * Dump IPv4 packet header
 *
 * @v iphdr	IPv4 header
 */
static void ipv4_dump ( struct iphdr *iphdr __unused ) {
	DBG ( "IP4 header at %p+%zx\n", iphdr, sizeof ( *iphdr ) );
	DBG ( "\tVersion = %d\n", ( iphdr->verhdrlen & IP_MASK_VER ) / 16 );
	DBG ( "\tHeader length = %d\n", iphdr->verhdrlen & IP_MASK_HLEN );
	DBG ( "\tService = %d\n", iphdr->service );
	DBG ( "\tTotal length = %d\n", ntohs ( iphdr->len ) );
	DBG ( "\tIdent = %d\n", ntohs ( iphdr->ident ) );
	DBG ( "\tFrags/Offset = %d\n", ntohs ( iphdr->frags ) );
	DBG ( "\tIP TTL = %d\n", iphdr->ttl );
	DBG ( "\tProtocol = %d\n", iphdr->protocol );
	DBG ( "\tHeader Checksum (at %p) = %x\n", &iphdr->chksum, 
				ntohs ( iphdr->chksum ) );
	DBG ( "\tSource = %s\n", inet_ntoa ( iphdr->src ) );
	DBG ( "\tDestination = %s\n", inet_ntoa ( iphdr->dest ) );
}

/**
 * Complete the transport-layer checksum
 */
void ipv4_tx_csum ( struct pk_buff *pkb, uint8_t trans_proto ) {

	struct iphdr *iphdr = pkb->data;
	void *pshdr = malloc ( IP_PSHLEN );
	void *csum_offset = iphdr + IP_HLEN + ( trans_proto == IP_UDP ? 6 : 16 );
	int offset = 0;

	/* Calculate pseudo header */
	memcpy ( pshdr, &iphdr->src, sizeof ( in_addr ) );
	offset += sizeof ( in_addr );
	memcpy ( pshdr + offset, &iphdr->dest, sizeof ( in_addr ) );
	offset += sizeof ( in_addr );
	*( ( uint8_t* ) ( pshdr + offset++ ) ) = 0x00;
	*( ( uint8_t* ) ( pshdr + offset++ ) ) = iphdr->protocol;
	*( ( uint16_t* ) ( pshdr + offset ) ) = pkb_len ( pkb ) - IP_HLEN;

	/* Update the checksum value */
	*( ( uint16_t* ) csum_offset ) = *( ( uint16_t* ) csum_offset ) + calc_chksum ( pshdr, IP_PSHLEN );
}

/**
 * Calculate the transport-layer checksum while processing packets
 */
uint16_t ipv4_rx_csum ( struct pk_buff *pkb __unused, uint8_t trans_proto __unused ) {
	/** This function needs to be implemented. Until then, it will return 0xffffffff every time */
	return 0xffff;
}

/**
 * Transmit packet constructed by uIP
 *
 * @v pkb		Packet buffer
 * @ret rc		Return status code
 *
 */
int ipv4_uip_tx ( struct pk_buff *pkb ) {
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
		/* Special case: IPv4 multicast over Ethernet.	This
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
	return net_tx ( pkb, netdev, &ipv4_protocol, ll_dest );

 err:
	free_pkb ( pkb );
	return rc;
}

/**
 * Transmit IP packet (without uIP)
 *
 * @v pkb		Packet buffer
 * @v trans_proto	Transport-layer protocol number
 * @v dest		Destination network-layer address
 * @ret rc		Status
 *
 * This function expects a transport-layer segment and prepends the IP header
 */
int ipv4_tx ( struct pk_buff *pkb, uint16_t trans_proto, struct in_addr *dest ) {
	struct iphdr *iphdr = pkb_push ( pkb, sizeof ( *iphdr ) );
	struct ipv4_miniroute *miniroute;
	struct net_device *netdev = NULL;
	struct in_addr next_hop;
	uint8_t ll_dest_buf[MAX_LL_ADDR_LEN];
	const uint8_t *ll_dest = ll_dest_buf;
	int rc;

	/* Fill up the IP header, except source address */
	iphdr->verhdrlen = ( IP_VER << 4 ) | ( sizeof ( *iphdr ) / 4 );
	iphdr->service = IP_TOS;
	iphdr->len = htons ( pkb_len ( pkb ) );	
	iphdr->ident = htons ( next_ident++ );
	iphdr->frags = 0;
	iphdr->ttl = IP_TTL;
	iphdr->protocol = trans_proto;

	/* Copy destination address */
	iphdr->dest = *dest;

	/**
	 * All fields in the IP header filled in except the source network
	 * address (which requires routing) and the header checksum (which
	 * requires the source network address). As the pseudo header requires
	 * the source address as well and the transport-layer checksum is
	 * updated after routing.
	 *
	 * Continue processing as in ipv4_uip_tx()
	 */

	/* Use routing table to identify next hop and transmitting netdev */
	next_hop = iphdr->dest;
	list_for_each_entry ( miniroute, &miniroutes, list ) {
		if ( ( ( ( iphdr->dest.s_addr ^ miniroute->address.s_addr ) &
			 miniroute->netmask.s_addr ) == 0 ) ||
		     ( miniroute->gateway.s_addr != INADDR_NONE ) ) {
			netdev = miniroute->netdev;
			iphdr->src = miniroute->address;
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

	/* Calculate the transport layer checksum */
	ipv4_tx_csum ( pkb, trans_proto );

	/* Calculate header checksum, in network byte order */
	iphdr->chksum = 0;
	iphdr->chksum = htons ( calc_chksum ( iphdr, sizeof ( *iphdr ) ) );

	/* Print IP4 header for debugging */
	ipv4_dump ( iphdr );

	/* Determine link-layer destination address */
	if ( next_hop.s_addr == INADDR_BROADCAST ) {
		/* Broadcast address */
		ll_dest = netdev->ll_protocol->ll_broadcast;
	} else if ( IN_MULTICAST ( next_hop.s_addr ) ) {
		/* Special case: IPv4 multicast over Ethernet.	This
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
					  &iphdr->src, ll_dest_buf ) ) != 0 ) {
			DBG ( "No ARP entry for %s\n",
			      inet_ntoa ( iphdr->dest ) );
			goto err;
		}
	}

	/* Hand off to link layer */
	return net_tx ( pkb, netdev, &ipv4_protocol, ll_dest );

 err:
	free_pkb ( pkb );
	return rc;
}

/**
 * Process incoming IP packets
 *
 * @v pkb		Packet buffer
 * @v netdev		Network device
 * @v ll_source		Link-layer source address
 * @ret rc		Return status code
 *
 * This handles IP packets by handing them off to the uIP protocol
 * stack.
 */
static int ipv4_uip_rx ( struct pk_buff *pkb,
			 struct net_device *netdev __unused,
			 const void *ll_source __unused ) {

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
		ipv4_uip_tx ( pkb );
	}
	return 0;
}

/**
 * Process incoming packets (without uIP)
 *
 * @v pkb	Packet buffer
 * @v netdev	Network device
 * @v ll_source	Link-layer destination source
 *
 * This function expects an IP4 network datagram. It processes the headers 
 * and sends it to the transport layer.
 */
void ipv4_rx ( struct pk_buff *pkb, struct net_device *netdev __unused,
			const void *ll_source __unused ) {
	struct iphdr *iphdr = pkb->data;
	struct in_addr *src = &iphdr->src;
	struct in_addr *dest = &iphdr->dest;
	uint16_t chksum;

	/* Sanity check */
	if ( pkb_len ( pkb ) < sizeof ( *iphdr ) ) {
		DBG ( "IP datagram too short (%d bytes)\n",
			pkb_len ( pkb ) );
		return;
	}

	/* Print IP4 header for debugging */
	ipv4_dump ( iphdr );

	/* Validate version and header length */
	if ( iphdr->verhdrlen != 0x45 ) {
		DBG ( "Bad version and header length %x\n", iphdr->verhdrlen );
		return;
	}

	/* Validate length of IP packet */
	if ( ntohs ( iphdr->len ) != pkb_len ( pkb ) ) {
		DBG ( "Inconsistent packet length %d\n",
					ntohs ( iphdr->len ) );
		return;
	}

	/* Verify the checksum */
	if ( ( chksum = ipv4_rx_csum ( pkb, iphdr->protocol ) )	!= 0xffff ) {
		DBG ( "Bad checksum %x\n", chksum );
	}

	/* To reduce code size, the following functions are not implemented:
	 * 1. Check the destination address
	 * 2. Check the TTL field
	 * 3. Check the service field
	 */

	/* Strip header */
	pkb_pull ( pkb, sizeof ( *iphdr ) );

	/* Send it to the transport layer */
	trans_rx ( pkb, iphdr->protocol, src, dest );
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
	struct ipv4_miniroute *miniroute;

	list_for_each_entry ( miniroute, &miniroutes, list ) {
		if ( ( miniroute->netdev == netdev ) &&
		     ( miniroute->address.s_addr == address->s_addr ) ) {
			/* Found matching address */
			return 0;
		}
	}
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
struct net_protocol ipv4_protocol = {
	.name = "IP",
	.net_proto = htons ( ETH_P_IP ),
	.net_addr_len = sizeof ( struct in_addr ),
#if USE_UIP
	.rx = ipv4_uip_rx,
#else
	.rx = ipv4_rx,
#endif
	.ntoa = ipv4_ntoa,
};

NET_PROTOCOL ( ipv4_protocol );

/** IPv4 ARP protocol */
struct arp_net_protocol ipv4_arp_protocol = {
	.net_protocol = &ipv4_protocol,
	.check = ipv4_arp_check,
};

ARP_NET_PROTOCOL ( ipv4_arp_protocol );
