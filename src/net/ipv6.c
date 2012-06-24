#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <byteswap.h>
#include <ipxe/in.h>
#include <ipxe/ip6.h>
#include <ipxe/ndp.h>
#include <ipxe/list.h>
#include <ipxe/icmp6.h>
#include <ipxe/tcpip.h>
#include <ipxe/socket.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>
#include <ipxe/if_ether.h>
#include <ipxe/timer.h>
#include <ipxe/retry.h>

#define is_linklocal( a ) ( ( (a).in6_u.u6_addr16[0] & htons ( 0xFE80 ) ) == htons ( 0xFE80 ) )

#define is_ext_hdr( nxt_hdr ) ( \
	( nxt_hdr == IP6_HOPBYHOP ) || \
	( nxt_hdr == IP6_PAD ) || \
	( nxt_hdr == IP6_PADN ) || \
	( nxt_hdr == IP6_ROUTING ) || \
	( nxt_hdr == IP6_FRAGMENT ) || \
	( nxt_hdr == IP6_AUTHENTICATION ) || \
	( nxt_hdr == IP6_DEST_OPTS ) || \
	( nxt_hdr == IP6_ESP ) || \
	( nxt_hdr == IP6_NO_HEADER ) )

char * inet6_ntoa ( struct in6_addr in6 );

static int ipv6_check ( struct net_device *netdev, const void *net_addr );

/* Unspecified IP6 address */
static struct in6_addr ip6_none = {
	.in6_u.u6_addr32 = { 0,0,0,0 }
};

/** List of IPv6 miniroutes */
struct list_head ipv6_miniroutes = LIST_HEAD_INIT ( ipv6_miniroutes );

/** List of fragment reassembly buffers */
static LIST_HEAD ( frag_buffers );

/**
 * Generate an EUI-64 from a given link-local address.
 *
 * @v out		pointer to buffer to receive the EUI-64
 * @v ll		pointer to buffer containing the link-local address
 */
void ipv6_generate_eui64 ( uint8_t *out, uint8_t *ll ) {
	assert ( out != 0 );
	assert ( ll != 0 );
	
	/* Create an EUI-64 identifier. */
	memcpy( out, ll, 3 );
	memcpy( out + 5, ll + 3, 3 );
	out[3] = 0xFF;
	out[4] = 0xFE;
	
	/* Designate that this is in fact an EUI-64. */
	out[0] |= 0x2;
}

/**
 * Verifies that a prefix matches another one.
 *
 * @v p1		first prefix
 * @v p2		second prefix
 * @v len		prefix length in bits to compare
 * @ret int		0 if a match, nonzero otherwise
 */
int ipv6_match_prefix ( struct in6_addr *p1, struct in6_addr *p2, size_t len ) {
	uint8_t ip1, ip2;
	size_t offset, bits;
	int rc = 0;

	/* Check for a prefix match on the route. */
	if ( ! memcmp ( p1, p2, len / 8 ) ) {
		rc = 0;

		/* Handle extra bits in the prefix. */
		if ( ( len % 8 ) ||
		     ( len < 8 ) ) {
			DBG ( "ipv6: prefix is not aligned to a byte.\n" );

			/* Compare the remaining bits. */
			offset = len / 8;
			bits = len % 8;

			ip1 = p1->in6_u.u6_addr8[offset];
			ip2 = p2->in6_u.u6_addr8[offset];
			if ( ! ( ( ip1 & (0xFF >> (8 - bits)) ) &
			     ( ip2 ) ) ) {
				rc = 1;
			}
		}
	} else {
		rc = 1;
	}

	return rc;
}

/**
 * Add IPv6 minirouting table entry
 *
 * @v netdev		Network device
 * @v prefix		Destination prefix (in bits)
 * @v address		Address of the interface
 * @v gateway		Gateway address (or ::0 for no gateway)
 * @ret miniroute	Routing table entry, or NULL
 */
static struct ipv6_miniroute * __malloc
add_ipv6_miniroute ( struct net_device *netdev, struct in6_addr prefix,
		     int prefix_len, struct in6_addr address,
		     struct in6_addr gateway ) {
	struct ipv6_miniroute *miniroute;
	
	DBG( "ipv6 add: %s/%d ", inet6_ntoa ( address ), prefix_len );
	DBG( "gw %s\n", inet6_ntoa( gateway ) );

	/* Try to find an already existent entry */
	list_for_each_entry ( miniroute, &ipv6_miniroutes, list ) {
		if ( miniroute->netdev == netdev &&
		     IP6_EQUAL ( miniroute->address, address ) &&
		     IP6_EQUAL ( miniroute->prefix, prefix ) ) {
			goto update;
		}
	}

	miniroute = malloc ( sizeof ( *miniroute ) );

	if ( !miniroute ) {
		return NULL;
	}

	/* Add miniroute to list of ipv6_miniroutes */
	if ( !IP6_EQUAL ( gateway, ip6_none ) ) {
		list_add_tail ( &miniroute->list, &ipv6_miniroutes );
	} else {
		list_add ( &miniroute->list, &ipv6_miniroutes );
	}
update:
	/* Record routing information */
	miniroute->netdev = netdev_get ( netdev );
	miniroute->prefix = prefix;
	miniroute->prefix_len = prefix_len;
	miniroute->address = address;
	miniroute->gateway = gateway;

	return miniroute;
}

/**
 * Delete IPv6 minirouting table entry
 *
 * @v miniroute		Routing table entry
 */
static void del_ipv6_miniroute ( struct ipv6_miniroute *miniroute ) {
	
	DBG ( "ipv6 del: %s/%d\n", inet6_ntoa(miniroute->address),
				   miniroute->prefix_len );
	
	netdev_put ( miniroute->netdev );
	list_del ( &miniroute->list );
	free ( miniroute );
}

/**
 * Add IPv6 interface
 *
 * @v netdev	Network device
 * @v prefix	Destination prefix
 * @v address	Address of the interface
 * @v gateway	Gateway address (or ::0 for no gateway)
 */
int add_ipv6_address ( struct net_device *netdev, struct in6_addr prefix,
		       int prefix_len, struct in6_addr address,
		       struct in6_addr gateway ) {
	struct ipv6_miniroute *miniroute;

	/* Clear any existing address for this net device */
	/* del_ipv6_address ( netdev ); */

	/* Add new miniroute */
	miniroute = add_ipv6_miniroute ( netdev, prefix, prefix_len, address,
					 gateway );
	if ( ! miniroute )
		return -ENOMEM;

	return 0;
}

/**
 * Remove IPv6 interface
 *
 * @v netdev	Network device
 */
void del_ipv6_address ( struct net_device *netdev ) {
	struct ipv6_miniroute *miniroute;

	list_for_each_entry ( miniroute, &ipv6_miniroutes, list ) {
		if ( miniroute->netdev == netdev ) {
			del_ipv6_miniroute ( miniroute );
			break;
		}
	}
}

/**
 * Calculate TX checksum
 *
 * @v iobuf	I/O buffer
 * @v csum	Partial checksum.
 *
 * This function constructs the pseudo header and completes the checksum in the
 * upper layer header.
 */
static uint16_t ipv6_tx_csum ( struct io_buffer *iobuf, uint16_t csum ) {
	struct ip6_header *ip6hdr = iobuf->data;
	struct ipv6_pseudo_header pshdr;

	/* Calculate pseudo header */
	memset ( &pshdr, 0, sizeof ( pshdr ) );
	pshdr.src = ip6hdr->src;
	pshdr.dest = ip6hdr->dest;
	pshdr.len = ip6hdr->payload_len;
	pshdr.nxt_hdr = ip6hdr->nxt_hdr;

	/* Update checksum value */
	return tcpip_continue_chksum ( csum, &pshdr, sizeof ( pshdr ) );
}

/**
 * Calculate TCPIP checksum with the given values
 *
 * @v csum	Partial checksum.
 * @v next_hdr	Next header in the packet.
 * @v length	Total data length, in host byte order.
 * @v src	Source address of the packet.
 * @v dst	Destination address of the packet.
 *
 * This function constructs the pseudo header and completes the checksum in the
 * upper layer header. It is to be used where an IP6 header is not available, or
 * fully valid, such as after fragment reassembly.
 */
static uint16_t ipv6_tx_csum_nohdr ( uint16_t csum, uint8_t next_hdr, uint16_t length,
				     struct in6_addr *src, struct in6_addr *dst ) {
	struct ipv6_pseudo_header pshdr;

	/* Calculate pseudo header */
	memset ( &pshdr, 0, sizeof ( pshdr ) );
	pshdr.src = *src;
	pshdr.dest = *dst;
	pshdr.len = htons ( length );
	pshdr.nxt_hdr = next_hdr;

	/* Update checksum value */
	return tcpip_continue_chksum ( csum, &pshdr, sizeof ( pshdr ) );
}

/**
 * Dump IP6 header for debugging
 *
 * ip6hdr	IPv6 header
 */
void ipv6_dump ( struct ip6_header *ip6hdr ) {
	/* Because inet6_ntoa returns a static char[16], each call needs to be
	 * separate. */
	DBG ( "IP6 %p src %s ", ip6hdr, inet6_ntoa( ip6hdr->src ) );
	DBG ( "dest %s nxt_hdr %d len %d\n", inet6_ntoa ( ip6hdr->dest ),
		  ip6hdr->nxt_hdr, ntohs ( ip6hdr->payload_len ) );
}

/**
 * Transmit IP6 packet
 *
 * iobuf		I/O buffer
 * tcpip	TCP/IP protocol
 * st_dest	Destination socket address
 *
 * This function prepends the IPv6 headers to the payload an transmits it.
 */
int ipv6_tx ( struct io_buffer *iobuf,
	      struct tcpip_protocol *tcpip,
	      struct sockaddr_tcpip *st_src __unused,
	      struct sockaddr_tcpip *st_dest,
	      struct net_device *netdev,
	      uint16_t *trans_csum ) {
	struct sockaddr_in6 *dest = ( struct sockaddr_in6* ) st_dest;
	struct in6_addr next_hop, gateway = ip6_none;
	struct ipv6_miniroute *miniroute;
	uint8_t ll_dest_buf[MAX_LL_ADDR_LEN];
	const uint8_t *ll_dest = ll_dest_buf;
	int rc, multicast, linklocal;
	
	/* Check for multicast transmission. */
	multicast = dest->sin6_addr.in6_u.u6_addr8[0] == 0xFF;

	/* Construct the IPv6 packet */
	struct ip6_header *ip6hdr = iob_push ( iobuf, sizeof ( *ip6hdr ) );
	memset ( ip6hdr, 0, sizeof ( *ip6hdr) );
	ip6hdr->ver_traffic_class_flow_label = htonl ( 0x60000000 );//IP6_VERSION;
	ip6hdr->payload_len = htons ( iob_len ( iobuf ) - sizeof ( *ip6hdr ) );
	ip6hdr->nxt_hdr = tcpip->tcpip_proto;
	ip6hdr->hop_limit = IP6_HOP_LIMIT; // 255
	
	/* Determine the next hop address and interface. */
	next_hop = dest->sin6_addr;
	list_for_each_entry ( miniroute, &ipv6_miniroutes, list ) {
		/* Check for specific netdev */
		if ( netdev && ( miniroute->netdev != netdev ) ) {
			continue;
		}
		
		/* Link-local route? */
		linklocal = is_linklocal ( miniroute->address ); // (.in6_u.u6_addr16[0] & htons(0xFE80)) == htons(0xFE80);

		/* Handle link-local for multicast. */
		if ( multicast )
		{
			/* Link-local scope? */
			if ( is_linklocal ( next_hop ) ) { // .in6_u.u6_addr8[0] & 0x2 ) {
				if ( linklocal ) {
					netdev = miniroute->netdev;
					ip6hdr->src = miniroute->address;
					break;
				} else {
					/* Should be link-local address. */
					continue;
				}
			} else {
				/* Assume we can TX on this interface, even if
				 * it is link-local. For multicast this should
				 * not be too much of a problem. */
				netdev = miniroute->netdev;
				ip6hdr->src = miniroute->address;
				break;
			}
		}
		
		/* Check for a prefix match on the route. */
		rc = ipv6_match_prefix ( &next_hop, &miniroute->prefix, miniroute->prefix_len );
		
		/* Matched? */
		if( rc == 0 ) {
			netdev = miniroute->netdev;
			ip6hdr->src = miniroute->address;
			break;
		}
		
		if ( ( ! ( IP6_EQUAL ( miniroute->gateway, ip6_none ) ) ) &&
		     ( IP6_EQUAL ( gateway, ip6_none ) ) ) {
			netdev = miniroute->netdev;
			ip6hdr->src = miniroute->address;
			gateway = miniroute->gateway;
		}
	}
	/* No network interface identified */
	if ( ( ! netdev ) ) {
		DBG ( "No route to host %s\n", inet6_ntoa ( dest->sin6_addr ) );
		rc = -ENETUNREACH;
		goto err;
	} else if ( ! IP6_EQUAL ( gateway, ip6_none ) ) {
		next_hop = gateway;
	}
	
	/* Add the next hop to the packet. */
	ip6hdr->dest = dest->sin6_addr;

	/* Complete the transport layer checksum */
	if ( trans_csum )
		*trans_csum = ipv6_tx_csum ( iobuf, *trans_csum );

	/* Print IPv6 header */
	/* ipv6_dump ( ip6hdr ); */

	/* Resolve link layer address */
	if ( next_hop.in6_u.u6_addr8[0] == 0xFF ) {
		ll_dest_buf[0] = 0x33;
		ll_dest_buf[1] = 0x33;
		ll_dest_buf[2] = next_hop.in6_u.u6_addr8[12];
		ll_dest_buf[3] = next_hop.in6_u.u6_addr8[13];
		ll_dest_buf[4] = next_hop.in6_u.u6_addr8[14];
		ll_dest_buf[5] = next_hop.in6_u.u6_addr8[15];
	} else {
		/* Unicast address needs to be resolved by NDP */
		if ( ( rc = ndp_resolve ( netdev, &next_hop, &ip6hdr->src,
					  ll_dest_buf ) ) != 0 ) {
			DBG ( "No entry for %s\n", inet6_ntoa ( next_hop ) );
			goto err;
		}
	}

	/* Transmit packet */
	return net_tx ( iobuf, netdev, &ipv6_protocol, ll_dest,
			netdev->ll_addr );

  err:
	free_iob ( iobuf );
	return rc;
}

/**
 * Fragment reassembly counter timeout
 *
 * @v timer	Retry timer
 * @v over	If asserted, the timer is greater than @c MAX_TIMEOUT 
 */
static void ipv6_frag_expired ( struct retry_timer *timer __unused,
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
	free ( fragbuf );
}

/**
 * Get the "next header" field and free a fragment buffer for a given iobuf.
 *
 * @v iobuf	I/O buffer to reference.
 */
static int frag_next_hdr ( struct io_buffer *iobuf ) {
	struct frag_buffer *fragbuf;
	int rc = IP6_NO_HEADER;
	
	list_for_each_entry ( fragbuf, &frag_buffers, list ) {
		if ( fragbuf->frag_iob == iobuf ) {
			rc = fragbuf->next_hdr;
			free_fragbuf ( fragbuf );
		}
	}
	
	return rc;
}

/**
 * Fragment reassembler
 *
 * @v iobuf		I/O buffer, fragment of the datagram
 * @ret frag_iob	Reassembled packet, or NULL
 */
static struct io_buffer * ipv6_reassemble ( struct io_buffer * iobuf,
				struct sockaddr_tcpip *st_src ) {
	struct ip6_frag_hdr *frag_hdr = iobuf->data;
	struct frag_buffer *fragbuf;
	
	struct sockaddr_in6 *src = ( struct sockaddr_in6 * ) st_src;
	
	/* Lift the flags and offset out. */
	uint16_t offset = ntohs ( frag_hdr->offset_flags ) & ~0x7;
	uint16_t flags = ntohs ( frag_hdr->offset_flags );
	
	/**
	 * Check if the fragment belongs to any fragment series
	 */
	list_for_each_entry ( fragbuf, &frag_buffers, list ) {
		if ( fragbuf->ident == frag_hdr->ident &&
		     IP6_EQUAL( fragbuf->src.s6_addr, src->sin6_addr ) ) {
			/**
			 * Check if the packet is the expected fragment
			 * 
			 * The offset of the new packet must be equal to the
			 * length of the data accumulated so far (the length of
			 * the reassembled I/O buffer
			 */
			if ( iob_len ( fragbuf->frag_iob ) == offset ) {
				/**
				 * Append the contents of the fragment to the
				 * reassembled I/O buffer
				 */
				iob_pull ( iobuf, sizeof ( *frag_hdr ) );
				memcpy ( iob_put ( fragbuf->frag_iob,
							iob_len ( iobuf ) ),
					 iobuf->data, iob_len ( iobuf ) );
				free_iob ( iobuf );

				/** Check if the fragment series is over */
				if ( ! ( flags & IP6_MORE_FRAGMENTS ) ) {
					iobuf = fragbuf->frag_iob;
					return iobuf;
				}

			} else {
				/* Discard the fragment series */
				free_fragbuf ( fragbuf );
				free_iob ( iobuf );
			}
			return NULL;
		}
	}
	
	/** Check if the fragment is the first in the fragment series */
	if ( ( flags & IP6_MORE_FRAGMENTS ) && ( offset == 0 ) ) {
	
		/** Create a new fragment buffer */
		fragbuf = ( struct frag_buffer* ) malloc ( sizeof( *fragbuf ) );
		fragbuf->ident = frag_hdr->ident;
		fragbuf->src = src->sin6_addr;
		fragbuf->next_hdr = frag_hdr->next_hdr;

		/* Set up the reassembly I/O buffer */
		fragbuf->frag_iob = alloc_iob ( IP6_FRAG_IOB_SIZE );
		iob_pull ( iobuf, sizeof ( *frag_hdr ) );
		memcpy ( iob_put ( fragbuf->frag_iob, iob_len ( iobuf ) ),
			 iobuf->data, iob_len ( iobuf ) );
		free_iob ( iobuf );

		/* Set the reassembly timer */
		timer_init ( &fragbuf->frag_timer, ipv6_frag_expired, NULL );
		start_timer_fixed ( &fragbuf->frag_timer, IP6_FRAG_TIMEOUT );

		/* Add the fragment buffer to the list of fragment buffers */
		list_add ( &fragbuf->list, &frag_buffers );
	}
	
	return NULL;
}

/**
 * Process next IP6 header
 *
 * @v iobuf	I/O buffer
 * @v nxt_hdr	Next header number
 * @v src	Source socket address
 * @v dest	Destination socket address
 * @v netdev	Net device the packet arrived on
 * @v phcsm Partial checksum over the IPv6 psuedo-header.
 *
 * Refer http://www.iana.org/assignments/ipv6-parameters for the numbers
 */
static int ipv6_process_nxt_hdr ( struct io_buffer *iobuf, uint8_t nxt_hdr,
		struct sockaddr_tcpip *src, struct sockaddr_tcpip *dest,
		struct net_device *netdev, uint16_t phcsm ) {
	struct io_buffer *reassembled;
	struct sockaddr_in6 *src_in = ( struct sockaddr_in6 * ) src;
	struct sockaddr_in6 *dest_in = ( struct sockaddr_in6 * ) dest;
	
	/* Special handling for fragments - to avoid having to recursively call
	 * this function in order to handle the packet. */
	if ( nxt_hdr == IP6_FRAGMENT ) {
		reassembled = ipv6_reassemble ( iobuf, src );
		if ( reassembled ) {
			/* Reassembled, pass to upper layer. */
			nxt_hdr = frag_next_hdr ( reassembled );
			iobuf = reassembled;
			if ( nxt_hdr == IP6_FRAGMENT ) {
				DBG ( "ip6: recursive fragment, dropping\n" );
				return -EINVAL;
			}
			
			/* Update the checksum. */
			phcsm = ipv6_tx_csum_nohdr ( TCPIP_EMPTY_CSUM,
					nxt_hdr, iob_len ( reassembled ),
					&src_in->sin6_addr, &dest_in->sin6_addr );
		} else {
			return 0;
		}
	}
	
	switch ( nxt_hdr ) {
	case IP6_PAD:
	case IP6_PADN:
		return 0; /* Padding options. */
	
	case IP6_AUTHENTICATION:
		/* Handle authentication. */
	case IP6_ESP:
		/* Handle an encapsulated security payload. */
		DBG ( "Function not implemented for header %d\n", nxt_hdr );
		return -ENOSYS;
	
	/* Can ignore these. */
	case IP6_HOPBYHOP:
	case IP6_ROUTING:
	case IP6_DEST_OPTS:
		DBG ( "ip6: ignoring header %d\n", nxt_hdr );
		break;
	case IP6_NO_HEADER:
		DBG ( "No next header\n" );
		return 0;
	case IP6_ICMP6:
		return icmp6_rx ( iobuf, netdev, src, dest, phcsm );
	}
	/* Next header is not a IPv6 extension header */
	return tcpip_rx ( iobuf, netdev, nxt_hdr, src, dest, phcsm );
}

/**
 * Iterate over IP6 headers, processing each one.
 *
 * @v iobuf	I/O buffer
 * @v src	Source socket address
 * @v dest	Destination socket address
 * @v netdev	Net device the packet arrived on
 * @v phcsm Partial checksum over the IPv6 psuedo-header.
 */
static int ipv6_process_headers ( struct io_buffer *iobuf, uint8_t nxt_hdr,
		struct sockaddr_tcpip *src, struct sockaddr_tcpip *dest,
		struct net_device *netdev, uint16_t phcsm ) {
	struct ip6_opt_hdr *opt = iobuf->data;
	int flags, rc = 0;
	
	/* Handle packets without options. */
	if ( ! is_ext_hdr ( nxt_hdr ) ) {
		return ipv6_process_nxt_hdr ( iobuf, nxt_hdr, src, dest,
					      netdev, phcsm );
	}
	
	/* Hop by hop header has a special indicator in nxt_hdr, that matches
	 * PAD and PADn options. So we special-case it. */
	if ( nxt_hdr == IP6_HOPBYHOP_FIRST ) {
		nxt_hdr = IP6_HOPBYHOP;
	}
	
	/* Iterate over the option list. */
	while ( iob_len ( iobuf ) ) {
		flags = nxt_hdr >> 6;
		
		DBG ( "about to process header %x\n", nxt_hdr );
		
		rc = ipv6_process_nxt_hdr ( iobuf, nxt_hdr,
					    src, dest, netdev, phcsm );
		if ( rc == -EINVAL ) { /* Invalid packet/header? */
			return rc;
		}
		
		/* Processing completes after a fragment is received. */
		if ( nxt_hdr == IP6_FRAGMENT ) {
			DBG ( "handled a fragment, breaking\n" );
			break;
		} else if ( ! is_ext_hdr ( nxt_hdr ) ) {
			DBG ( "no more extension headers, iob probably invalid!\n" );
			break;
		}
		
		if ( rc != 0 ) { /* Ignore all other errors. */
			DBG ( "ip6: unsupported extension header encountered, ignoring\n" );
			rc = 0;
		}
	
		nxt_hdr = opt->type;
		opt = iob_pull ( iobuf, opt->len );
		
		/* Stop processing if there are no more headers. */
		if ( nxt_hdr == IP6_NO_HEADER ) {
			break;
		}
	}
	
	return rc;
}

/**
 * Process incoming IP6 packets
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v ll_dest		Link-layer destination address
 * @v ll_source		Link-layer source address
 * @v flags		Packet flags
 *
 * This function processes a IPv6 packet
 */
static int ipv6_rx ( struct io_buffer *iobuf,
		     __unused struct net_device *netdev,
		     __unused const void *ll_dest,
		     __unused const void *ll_source,
		     __unused unsigned int flags ) {

	struct ip6_header *ip6hdr = iobuf->data;
	union {
		struct sockaddr_in6 sin6;
		struct sockaddr_tcpip st;
	} src, dest;
	uint16_t phcsm = 0;
	int rc = 0;

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *ip6hdr ) ) {
		DBG ( "Packet too short (%zd bytes)\n", iob_len ( iobuf ) );
		rc = -EINVAL;
		goto drop;
	}

	/* Construct socket address */
	memset ( &src, 0, sizeof ( src ) );
	src.sin6.sin_family = AF_INET6;
	src.sin6.sin6_addr = ip6hdr->src;
	memset ( &dest, 0, sizeof ( dest ) );
	dest.sin6.sin_family = AF_INET6;
	dest.sin6.sin6_addr = ip6hdr->dest;

	/* Check destination - always allow multicast. */
	if ( dest.sin6.sin6_addr.s6_addr[0] != 0xFF ) {
		if ( ipv6_check ( netdev, &dest.sin6.sin6_addr ) ) {
			DBG ( "IP6: packet not for us\n" );
			rc = -EINVAL;
			goto drop;
		}
	}

	/* Print IP6 header for debugging */
	ipv6_dump ( ip6hdr );

	/* Check header version */
	if ( ( ntohl( ip6hdr->ver_traffic_class_flow_label ) & 0xf0000000 ) != 0x60000000 ) {
		DBG ( "Invalid protocol version\n" );
		rc = -EINVAL;
		goto drop;
	}

	/* Check the payload length */
	if ( ntohs ( ip6hdr->payload_len ) > iob_len ( iobuf ) ) {
		DBG ( "Inconsistent packet length (%d bytes)\n",
			ip6hdr->payload_len );
		rc = -EINVAL;
		goto drop;
	}

	/* Ignore the traffic class and flow control values */

	/* Calculate the psuedo-header checksum before the IP6 header is
	 * stripped away. */
	phcsm = ipv6_tx_csum ( iobuf, TCPIP_EMPTY_CSUM );

	/* Strip header */
	iob_unput ( iobuf, iob_len ( iobuf ) - ntohs ( ip6hdr->payload_len ) -
							sizeof ( *ip6hdr ) );
	iob_pull ( iobuf, sizeof ( *ip6hdr ) );

	/* Send it to the transport layer */
	return ipv6_process_headers ( iobuf, ip6hdr->nxt_hdr, &src.st,
				      &dest.st, netdev, phcsm );

  drop:
	DBG ( "IP6 packet dropped\n" );
	free_iob ( iobuf );
	return rc;
}

/**
 * Convert an IPv6 address to a string.
 *
 * @v in6   Address to convert to string.
 *
 * Converts an IPv6 address to a string, and applies zero-compression as needed
 * to condense the address for easier reading/typing.
 */
char * inet6_ntoa ( struct in6_addr in6 ) {
	static char buf[40];
	uint16_t *bytes = ( uint16_t* ) &in6;
	size_t i = 0, longest = 0, tmp = 0, long_idx = ~0;
	
	/* ::0 */
	if ( IP6_EQUAL ( in6, ip6_none ) ) {
		tmp = sprintf ( buf, "::0" );
		buf[tmp] = 0;
		return buf;
	}

	/* Determine the longest string of zeroes for zero-compression. */
	for ( ; i < 8; i++ ) {
		if ( !bytes[i] )
			tmp++;
		else if(tmp > longest) {
			longest = tmp;
			long_idx = i - longest;
			
			tmp = 0;
		}
	}
	
	/* Check for last word being zero. This will cause long_idx to be zero,
	 * which confuses the actual buffer fill code. */
	if(tmp && (tmp > longest)) {
		longest = tmp;
		long_idx = 8 - longest;
	}

	/* Inject into the buffer. */
	tmp = 0;
	for ( i = 0; i < 8; i++ ) {
		/* Should we skip over a string of zeroes? */
		if ( i == long_idx ) {
			i += longest;
			tmp += sprintf( buf + tmp, ":" );

			/* Handle end-of-string. */
			if(i > 7)
				break;
		}

		/* Insert this component of the address. */
		tmp += sprintf(buf + tmp, "%x", ntohs(bytes[i]));

		/* Add the next colon, if needed. */
		if ( i < 7 )
			tmp += sprintf( buf + tmp, ":" );
	}

	buf[tmp] = 0;

	return buf;
}

/**
 * Convert a string to an IPv6 address.
 *
 * @v in6   String to convert to an address.
 */
int inet6_aton ( const char *cp, struct in6_addr *inp ) {
	char convbuf[40];
	char *tmp = convbuf, *next = convbuf;
	size_t i = 0;
	int ok;
	char c;
	
	/* Verify a valid address. */
	while ( ( c = cp[i++] ) ) {
		ok = c == ':';
		ok = ok || ( ( c >= '0' ) && ( c <= '9' ) );
		ok = ok || ( ( c >= 'a' ) && ( c <= 'f' ) );
		ok = ok || ( ( c >= 'A' ) && ( c <= 'F' ) );
		
		if ( ! ok ) {
			return 0;
		}
	}
	if ( ! i ) {
		return 0;
	}
	
	strcpy ( convbuf, cp );
	
	DBG ( "ipv6 converting %s to an in6_addr\n", cp );
	
	/* Handle the first part of the address (or all of it if no zero-compression. */
	i = 0;
	while ( ( next = strchr ( next, ':' ) ) ) {
		/* Cater for zero-compression. */
		if ( *tmp == ':' )
			break;
		
		/* Convert to integer. */
		inp->s6_addr16[i++] = htons( strtoul ( tmp, 0, 16 ) );
		
		*next++ = 0;
		tmp = next;
	}
	
	/* Handle the case where no zero-compression is needed, but every word
	 * was filled in the address. */
	if ( ( i == 7 ) && ( *tmp != ':' ) ) {
		inp->s6_addr16[i++] = htons( strtoul ( tmp, 0, 16 ) );
	}
	else
	{
		/* Handle zero-compression now (go backwards). */
		i = 7;
		if ( i && ( *tmp == ':' ) ) {
			next = strrchr ( next, ':' );
			do
			{
				tmp = next + 1;
				*next-- = 0;
	
				/* Convert to integer. */
				inp->s6_addr16[i--] = htons( strtoul ( tmp, 0, 16 ) );
			} while ( ( next = strrchr ( next, ':' ) ) );
		}
	}
	
	return 1;
}

static const char * ipv6_ntoa ( const void *net_addr ) {
	return inet6_ntoa ( * ( ( struct in6_addr * ) net_addr ) );
}

static int ipv6_check ( struct net_device *netdev, const void *net_addr ) {
	const struct in6_addr *address = net_addr;
	struct ipv6_miniroute *miniroute;

	list_for_each_entry ( miniroute, &ipv6_miniroutes, list ) {
		if ( ( miniroute->netdev == netdev ) &&
		     ( ! memcmp ( &miniroute->address, address, 16 ) ) ) {
			/* Found matching address */
			return 0;
		}
	}
	return -ENOENT;
}

/** IPv6 protocol */
struct net_protocol ipv6_protocol __net_protocol = {
	.name = "IPv6",
	.net_proto = htons ( ETH_P_IPV6 ),
	.net_addr_len = sizeof ( struct in6_addr ),
	.rx = ipv6_rx,
	.ntoa = ipv6_ntoa,
};

/** IPv6 TCPIP net protocol */
struct tcpip_net_protocol ipv6_tcpip_protocol __tcpip_net_protocol = {
	.name = "IPv6",
	.sa_family = AF_INET6,
	.tx = ipv6_tx,
};

/******************************************************************************
 *
 * Settings
 *
 ******************************************************************************
 */

/** IPv6 address setting */
struct setting ip6_setting __setting ( SETTING_IPv6 ) = {
	.name = "ip6",
	.description = "IPv6 address",
	.tag = 0, //DHCP6_OPT_IAADDR,
	.type = &setting_type_ipv6,
};

/** IPv6 prefix setting */
struct setting prefix_setting __setting ( SETTING_IPv6 ) = {
	.name = "prefix",
	.description = "IPv6 address prefix length",
	.tag = 0,
	.type = &setting_type_int32,
};

/** Default IPv6 gateway setting */
struct setting gateway6_setting __setting ( SETTING_IPv6 ) = {
	.name = "gateway6",
	.description = "IPv6 Default gateway",
	.tag = 0,
	.type = &setting_type_ipv6,
};

/**
 * Create IPv6 routes based on configured settings.
 *
 * @ret rc		Return status code
 */
static int ipv6_create_routes ( void ) {
	struct ipv6_miniroute *miniroute;
	struct ipv6_miniroute *tmp;
	struct net_device *netdev;
	struct settings *settings;
	struct in6_addr address;
	struct in6_addr gateway;
	long prefix = 0;
	int rc = 0;
	
	/* Create a route for each configured network device */
	for_each_netdev ( netdev ) {
		settings = netdev_settings ( netdev );
	
		/* Read the settings first. We may need to clear routes. */
		fetch_ipv6_setting ( settings, &ip6_setting, &address );
		fetch_ipv6_setting ( settings, &gateway6_setting, &gateway );
		fetch_int_setting ( settings, &prefix_setting, &prefix );
	
		/* Sanity check! */
		if ( ( prefix <= 0 ) || ( prefix > 128 ) ) {
			DBG ( "ipv6: attempt to apply settings without a valid prefix, ignoring\n" );
			continue; /* Simply ignore this setting. */
		}
	
		/* Remove any existing routes for this address. */
		list_for_each_entry_safe ( miniroute, tmp, &ipv6_miniroutes, list ) {
			if ( ! ipv6_match_prefix ( &address,
						 &miniroute->prefix,
						 prefix ) ) {
				DBG ( "ipv6: existing route for a configured setting, deleting\n" );
				del_ipv6_miniroute ( miniroute );
			}
		}
		
		/* Configure route */
		rc = add_ipv6_address ( netdev, address, prefix,
					address, gateway );
		if ( ! rc )
			return rc;
	}
	
	return 0;
}

/** IPv6 settings applicator */
struct settings_applicator ipv6_settings_applicator __settings_applicator = {
	.apply = ipv6_create_routes,
};

/* Drag in ICMP6 */
REQUIRE_OBJECT ( icmpv6 );
