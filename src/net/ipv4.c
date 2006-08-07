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
#include <gpxe/tcpip.h>

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

/** List of fragment reassembly buffers */
static LIST_HEAD ( frag_buffers );

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

	DBG ( "IP4 header at %p+%#zx\n", iphdr, sizeof ( *iphdr ) );
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
 * Fragment reassembly counter timeout
 *
 * @v timer	Retry timer
 * @v over	If asserted, the timer is greater than @c MAX_TIMEOUT 
 */
static void ipv4_frag_expired ( struct retry_timer *timer __unused,
				int over ) {
	if ( over ) {
		DBG ( "Fragment reassembly timeout" );
		/* Free the fragment buffer */
	}
}

/**
 * Free fragment buffer
 *
 * @v fragbug	Fragment buffer
 */
static void free_fragbuf ( struct frag_buffer *fragbuf ) {
	if ( fragbuf ) {
		free_dma ( fragbuf, sizeof ( *fragbuf ) );
	}
}

/**
 * Fragment reassembler
 *
 * @v pkb		Packet buffer, fragment of the datagram
 * @ret frag_pkb	Reassembled packet, or NULL
 */
static struct pk_buff * ipv4_reassemble ( struct pk_buff * pkb ) {
	struct iphdr *iphdr = pkb->data;
	struct frag_buffer *fragbuf;
	
	/**
	 * Check if the fragment belongs to any fragment series
	 */
	list_for_each_entry ( fragbuf, &frag_buffers, list ) {
		if ( fragbuf->ident == iphdr->ident &&
		     fragbuf->src.s_addr == iphdr->src.s_addr ) {
			/**
			 * Check if the packet is the expected fragment
			 * 
			 * The offset of the new packet must be equal to the
			 * length of the data accumulated so far (the length of
			 * the reassembled packet buffer
			 */
			if ( pkb_len ( fragbuf->frag_pkb ) == 
			      ( iphdr->frags & IP_MASK_OFFSET ) ) {
				/**
				 * Append the contents of the fragment to the
				 * reassembled packet buffer
				 */
				pkb_pull ( pkb, sizeof ( *iphdr ) );
				memcpy ( pkb_put ( fragbuf->frag_pkb,
							pkb_len ( pkb ) ),
					 pkb->data, pkb_len ( pkb ) );
				free_pkb ( pkb );

				/** Check if the fragment series is over */
				if ( !iphdr->frags & IP_MASK_MOREFRAGS ) {
					pkb = fragbuf->frag_pkb;
					free_fragbuf ( fragbuf );
					return pkb;
				}

			} else {
				/* Discard the fragment series */
				free_fragbuf ( fragbuf );
				free_pkb ( pkb );
			}
			return NULL;
		}
	}
	
	/** Check if the fragment is the first in the fragment series */
	if ( iphdr->frags & IP_MASK_MOREFRAGS &&
			( ( iphdr->frags & IP_MASK_OFFSET ) == 0 ) ) {
	
		/** Create a new fragment buffer */
		fragbuf = ( struct frag_buffer* ) malloc ( sizeof( *fragbuf ) );
		fragbuf->ident = iphdr->ident;
		fragbuf->src = iphdr->src;

		/* Set up the reassembly packet buffer */
		fragbuf->frag_pkb = alloc_pkb ( IP_FRAG_PKB_SIZE );
		pkb_pull ( pkb, sizeof ( *iphdr ) );
		memcpy ( pkb_put ( fragbuf->frag_pkb, pkb_len ( pkb ) ),
			 pkb->data, pkb_len ( pkb ) );
		free_pkb ( pkb );

		/* Set the reassembly timer */
		fragbuf->frag_timer.timeout = IP_FRAG_TIMEOUT;
		fragbuf->frag_timer.expired = ipv4_frag_expired;
		start_timer ( &fragbuf->frag_timer );

		/* Add the fragment buffer to the list of fragment buffers */
		list_add ( &fragbuf->list, &frag_buffers );
	}
	
	return NULL;
}


/**
 * Complete the transport-layer checksum
 *
 * @v pkb	Packet buffer
 * @v tcpip	Transport-layer protocol
 *
 * This function calculates the tcpip 
 */
static void ipv4_tx_csum ( struct pk_buff *pkb,
			   struct tcpip_protocol *tcpip ) {
	struct iphdr *iphdr = pkb->data;
	struct ipv4_pseudo_header pshdr;
	uint16_t *csum = ( ( ( void * ) iphdr ) + sizeof ( *iphdr )
			   + tcpip->csum_offset );

	/* Calculate pseudo header */
	pshdr.src = iphdr->src;
	pshdr.dest = iphdr->dest;
	pshdr.zero_padding = 0x00;
	pshdr.protocol = iphdr->protocol;
	/* This is only valid when IPv4 does not have options */
	pshdr.len = htons ( pkb_len ( pkb ) - sizeof ( *iphdr ) );

	/* Update the checksum value */
	*csum = tcpip_continue_chksum ( *csum, &pshdr, sizeof ( pshdr ) );
}

/**
 * Calculate the transport-layer checksum while processing packets
 */
static uint16_t ipv4_rx_csum ( struct pk_buff *pkb __unused,
			       uint8_t trans_proto __unused ) {
	/** 
	 * This function needs to be implemented. Until then, it will return
	 * 0xffffffff every time
	 */
	return 0xffff;
}

/**
 * Transmit IP packet
 *
 * @v pkb		Packet buffer
 * @v tcpip		Transport-layer protocol
 * @v st_dest		Destination network-layer address
 * @ret rc		Status
 *
 * This function expects a transport-layer segment and prepends the IP header
 */
static int ipv4_tx ( struct pk_buff *pkb,
		     struct tcpip_protocol *tcpip_protocol,
		     struct sockaddr_tcpip *st_dest ) {
	struct iphdr *iphdr = pkb_push ( pkb, sizeof ( *iphdr ) );
	struct sockaddr_in *sin_dest = ( ( struct sockaddr_in * ) st_dest );
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
	iphdr->protocol = tcpip_protocol->tcpip_proto;

	/* Copy destination address */
	iphdr->dest = sin_dest->sin_addr;

	/**
	 * All fields in the IP header filled in except the source network
	 * address (which requires routing) and the header checksum (which
	 * requires the source network address). As the pseudo header requires
	 * the source address as well and the transport-layer checksum is
	 * updated after routing.
	 */

	/* Use routing table to identify next hop and transmitting netdev */
	next_hop = iphdr->dest;
	list_for_each_entry ( miniroute, &miniroutes, list ) {
		int local, has_gw;

		local = ( ( ( iphdr->dest.s_addr ^ miniroute->address.s_addr )
			    & miniroute->netmask.s_addr ) == 0 );
		has_gw = ( miniroute->gateway.s_addr != INADDR_NONE );
		if ( local || has_gw ) {
			netdev = miniroute->netdev;
			iphdr->src = miniroute->address;
			if ( ! local )
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
	if ( tcpip_protocol->csum_offset > 0 ) {
		ipv4_tx_csum ( pkb, tcpip_protocol );
	}

	/* Calculate header checksum, in network byte order */
	iphdr->chksum = 0;
	iphdr->chksum = tcpip_chksum ( iphdr, sizeof ( *iphdr ) );

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
 * Process incoming packets
 *
 * @v pkb	Packet buffer
 * @v netdev	Network device
 * @v ll_source	Link-layer destination source
 *
 * This function expects an IP4 network datagram. It processes the headers 
 * and sends it to the transport layer.
 */
static int ipv4_rx ( struct pk_buff *pkb, struct net_device *netdev __unused,
		     const void *ll_source __unused ) {
	struct iphdr *iphdr = pkb->data;
	union {
		struct sockaddr_in sin;
		struct sockaddr_tcpip st;
	} src, dest;
	uint16_t chksum;

	/* Sanity check */
	if ( pkb_len ( pkb ) < sizeof ( *iphdr ) ) {
		DBG ( "IP datagram too short (%d bytes)\n",
			pkb_len ( pkb ) );
		return -EINVAL;
	}

	/* Print IP4 header for debugging */
	ipv4_dump ( iphdr );

	/* Validate version and header length */
	if ( iphdr->verhdrlen != 0x45 ) {
		DBG ( "Bad version and header length %x\n", iphdr->verhdrlen );
		return -EINVAL;
	}

	/* Validate length of IP packet */
	if ( ntohs ( iphdr->len ) > pkb_len ( pkb ) ) {
		DBG ( "Inconsistent packet length %d\n",
		      ntohs ( iphdr->len ) );
		return -EINVAL;
	}

	/* Verify the checksum */
	if ( ( chksum = ipv4_rx_csum ( pkb, iphdr->protocol ) )	!= 0xffff ) {
		DBG ( "Bad checksum %x\n", chksum );
	}
	/* Fragment reassembly */
	if ( iphdr->frags & IP_MASK_MOREFRAGS || 
		( !iphdr->frags & IP_MASK_MOREFRAGS &&
			iphdr->frags & IP_MASK_OFFSET != 0 ) ) {
		/* Pass the fragment to the reassembler ipv4_ressable() which
		 * either returns a fully reassembled packet buffer or NULL.
		 */
		pkb = ipv4_reassemble ( pkb );
		if ( !pkb ) {
			return 0;
		}
	}

	/* To reduce code size, the following functions are not implemented:
	 * 1. Check the destination address
	 * 2. Check the TTL field
	 * 3. Check the service field
	 */

	/* Construct socket addresses */
	memset ( &src, 0, sizeof ( src ) );
	src.sin.sin_family = AF_INET;
	src.sin.sin_addr = iphdr->src;
	memset ( &dest, 0, sizeof ( dest ) );
	dest.sin.sin_family = AF_INET;
	dest.sin.sin_addr = iphdr->dest;

	/* Strip header */
	pkb_pull ( pkb, sizeof ( *iphdr ) );

	/* Send it to the transport layer */
	return tcpip_rx ( pkb, iphdr->protocol, &src.st, &dest.st );
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
	.rx = ipv4_rx,
	.ntoa = ipv4_ntoa,
};

NET_PROTOCOL ( ipv4_protocol );

/** IPv4 TCPIP net protocol */
struct tcpip_net_protocol ipv4_tcpip_protocol = {
	.name = "IPv4",
	.sa_family = AF_INET,
	.tx = ipv4_tx,
};

TCPIP_NET_PROTOCOL ( ipv4_tcpip_protocol );

/** IPv4 ARP protocol */
struct arp_net_protocol ipv4_arp_protocol = {
	.net_protocol = &ipv4_protocol,
	.check = ipv4_arp_check,
};

ARP_NET_PROTOCOL ( ipv4_arp_protocol );
