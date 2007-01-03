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
 * Perform IPv4 routing
 *
 * @v dest		Final destination address
 * @ret dest		Next hop destination address
 * @ret miniroute	Routing table entry to use, or NULL if no route
 */
static struct ipv4_miniroute * ipv4_route ( struct in_addr *dest ) {
	struct ipv4_miniroute *miniroute;
	int local;
	int has_gw;

	list_for_each_entry ( miniroute, &miniroutes, list ) {
		local = ( ( ( dest->s_addr ^ miniroute->address.s_addr )
			    & miniroute->netmask.s_addr ) == 0 );
		has_gw = ( miniroute->gateway.s_addr != INADDR_NONE );
		if ( local || has_gw ) {
			if ( ! local )
				*dest = miniroute->gateway;
			return miniroute;
		}
	}

	return NULL;
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
 * Add IPv4 pseudo-header checksum to existing checksum
 *
 * @v pkb		Packet buffer
 * @v csum		Existing checksum
 * @ret csum		Updated checksum
 */
static uint16_t ipv4_pshdr_chksum ( struct pk_buff *pkb, uint16_t csum ) {
	struct ipv4_pseudo_header pshdr;
	struct iphdr *iphdr = pkb->data;
	size_t hdrlen = ( ( iphdr->verhdrlen & IP_MASK_HLEN ) * 4 );

	/* Build pseudo-header */
	pshdr.src = iphdr->src;
	pshdr.dest = iphdr->dest;
	pshdr.zero_padding = 0x00;
	pshdr.protocol = iphdr->protocol;
	pshdr.len = htons ( pkb_len ( pkb ) - hdrlen );

	/* Update the checksum value */
	return tcpip_continue_chksum ( csum, &pshdr, sizeof ( pshdr ) );
}

/**
 * Determine link-layer address
 *
 * @v dest		IPv4 destination address
 * @v src		IPv4 source address
 * @v netdev		Network device
 * @v ll_dest		Link-layer destination address buffer
 * @ret rc		Return status code
 */
static int ipv4_ll_addr ( struct in_addr dest, struct in_addr src,
			  struct net_device *netdev, uint8_t *ll_dest ) {
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	uint8_t *dest_bytes = ( ( uint8_t * ) &dest );

	if ( dest.s_addr == INADDR_BROADCAST ) {
		/* Broadcast address */
		memcpy ( ll_dest, ll_protocol->ll_broadcast,
			 ll_protocol->ll_addr_len );
		return 0;
	} else if ( IN_MULTICAST ( dest.s_addr ) ) {
		/* Special case: IPv4 multicast over Ethernet.	This
		 * code may need to be generalised once we find out
		 * what happens for other link layers.
		 */
		ll_dest[0] = 0x01;
		ll_dest[1] = 0x00;
		ll_dest[2] = 0x5e;
		ll_dest[3] = dest_bytes[1] & 0x7f;
		ll_dest[4] = dest_bytes[2];
		ll_dest[5] = dest_bytes[3];
		return 0;
	} else {
		/* Unicast address: resolve via ARP */
		return arp_resolve ( netdev, &ipv4_protocol, &dest,
				     &src, ll_dest );
	}
}

/**
 * Transmit IP packet
 *
 * @v pkb		Packet buffer
 * @v tcpip		Transport-layer protocol
 * @v st_dest		Destination network-layer address
 * @v trans_csum	Transport-layer checksum to complete, or NULL
 * @ret rc		Status
 *
 * This function expects a transport-layer segment and prepends the IP header
 */
static int ipv4_tx ( struct pk_buff *pkb,
		     struct tcpip_protocol *tcpip_protocol,
		     struct sockaddr_tcpip *st_dest, uint16_t *trans_csum ) {
	struct iphdr *iphdr = pkb_push ( pkb, sizeof ( *iphdr ) );
	struct sockaddr_in *sin_dest = ( ( struct sockaddr_in * ) st_dest );
	struct ipv4_miniroute *miniroute;
	struct in_addr next_hop;
	uint8_t ll_dest[MAX_LL_ADDR_LEN];
	int rc;

	/* Fill up the IP header, except source address */
	iphdr->verhdrlen = ( IP_VER | ( sizeof ( *iphdr ) / 4 ) );
	iphdr->service = IP_TOS;
	iphdr->len = htons ( pkb_len ( pkb ) );	
	iphdr->ident = htons ( ++next_ident );
	iphdr->frags = 0;
	iphdr->ttl = IP_TTL;
	iphdr->protocol = tcpip_protocol->tcpip_proto;
	iphdr->chksum = 0;
	iphdr->dest = sin_dest->sin_addr;

	/* Use routing table to identify next hop and transmitting netdev */
	next_hop = iphdr->dest;
	miniroute = ipv4_route ( &next_hop );
	if ( ! miniroute ) {
		DBG ( "IPv4 has no route to %s\n", inet_ntoa ( iphdr->dest ) );
		rc = -EHOSTUNREACH;
		goto err;
	}
	iphdr->src = miniroute->address;

	/* Determine link-layer destination address */
	if ( ( rc = ipv4_ll_addr ( next_hop, iphdr->src, miniroute->netdev,
				   ll_dest ) ) != 0 ) {
		DBG ( "IPv4 has no link-layer address for %s\n",
		      inet_ntoa ( iphdr->dest ) );
		goto err;
	}

	/* Fix up checksums */
	if ( trans_csum )
		*trans_csum = ipv4_pshdr_chksum ( pkb, *trans_csum );
	iphdr->chksum = tcpip_chksum ( iphdr, sizeof ( *iphdr ) );

	/* Print IP4 header for debugging */
	DBG ( "IPv4 TX %s->", inet_ntoa ( iphdr->src ) );
	DBG ( "%s len %d proto %d id %04x csum %04x\n",
	      inet_ntoa ( iphdr->dest ), ntohs ( iphdr->len ), iphdr->protocol,
	      ntohs ( iphdr->ident ), ntohs ( iphdr->chksum ) );

	/* Hand off to link layer */
	return net_tx ( pkb, miniroute->netdev, &ipv4_protocol, ll_dest );

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
	size_t hdrlen;
	size_t len;
	union {
		struct sockaddr_in sin;
		struct sockaddr_tcpip st;
	} src, dest;
	uint16_t csum;
	uint16_t pshdr_csum;

	/* Sanity check the IPv4 header */
	if ( pkb_len ( pkb ) < sizeof ( *iphdr ) ) {
		DBG ( "IPv4 packet too short at %d bytes (min %d bytes)\n",
		      pkb_len ( pkb ), sizeof ( *iphdr ) );
		goto err;
	}
	if ( ( iphdr->verhdrlen & IP_MASK_VER ) != IP_VER ) {
		DBG ( "IPv4 version %#02x not supported\n", iphdr->verhdrlen );
		goto err;
	}
	hdrlen = ( ( iphdr->verhdrlen & IP_MASK_HLEN ) * 4 );
	if ( hdrlen < sizeof ( *iphdr ) ) {
		DBG ( "IPv4 header too short at %d bytes (min %d bytes)\n",
		      hdrlen, sizeof ( *iphdr ) );
		goto err;
	}
	if ( hdrlen > pkb_len ( pkb ) ) {
		DBG ( "IPv4 header too long at %d bytes "
		      "(packet is %d bytes)\n", hdrlen, pkb_len ( pkb ) );
		goto err;
	}
	if ( ( csum = tcpip_chksum ( iphdr, hdrlen ) ) != 0 ) {
		DBG ( "IPv4 checksum incorrect (is %04x including checksum "
		      "field, should be 0000)\n", csum );
		goto err;
	}
	len = ntohs ( iphdr->len );
	if ( len < hdrlen ) {
		DBG ( "IPv4 length too short at %d bytes "
		      "(header is %d bytes)\n", len, hdrlen );
		goto err;
	}
	if ( len > pkb_len ( pkb ) ) {
		DBG ( "IPv4 length too long at %d bytes "
		      "(packet is %d bytes)\n", len, pkb_len ( pkb ) );
		goto err;
	}

	/* Print IPv4 header for debugging */
	DBG ( "IPv4 RX %s<-", inet_ntoa ( iphdr->dest ) );
	DBG ( "%s len %d proto %d id %04x csum %04x\n",
	      inet_ntoa ( iphdr->src ), ntohs ( iphdr->len ), iphdr->protocol,
	      ntohs ( iphdr->ident ), ntohs ( iphdr->chksum ) );

	/* Truncate packet to correct length, calculate pseudo-header
	 * checksum and then strip off the IPv4 header.
	 */
	pkb_unput ( pkb, ( pkb_len ( pkb ) - len ) );
	pshdr_csum = ipv4_pshdr_chksum ( pkb, TCPIP_EMPTY_CSUM );
	pkb_pull ( pkb, hdrlen );

	/* Fragment reassembly */
	if ( ( iphdr->frags & htons ( IP_MASK_MOREFRAGS ) ) || 
	     ( ( iphdr->frags & htons ( IP_MASK_OFFSET ) ) != 0 ) ) {
		/* Pass the fragment to ipv4_reassemble() which either
		 * returns a fully reassembled packet buffer or NULL.
		 */
		pkb = ipv4_reassemble ( pkb );
		if ( ! pkb )
			return 0;
	}

	/* Construct socket addresses and hand off to transport layer */
	memset ( &src, 0, sizeof ( src ) );
	src.sin.sin_family = AF_INET;
	src.sin.sin_addr = iphdr->src;
	memset ( &dest, 0, sizeof ( dest ) );
	dest.sin.sin_family = AF_INET;
	dest.sin.sin_addr = iphdr->dest;
	return tcpip_rx ( pkb, iphdr->protocol, &src.st, &dest.st, pshdr_csum);

 err:
	free_pkb ( pkb );
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
