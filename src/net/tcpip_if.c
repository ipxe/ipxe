#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <byteswap.h>
#include <gpxe/in.h>
#include <gpxe/ip.h>
#include <gpxe/pkbuff.h>
#include <gpxe/tables.h>
#include <gpxe/netdevice.h>
#include <gpxe/tcpip_if.h>

/** @file
 *
 * Transport-network layer interface
 *
 * This file contains functions and utilities for the transport-network layer interface
 */

/** Registered network-layer protocols that support TCPIP */
static struct tcpip_net_protocol tcpip_net_protocols[0] __table_start ( tcpip_net_protocols );
static struct tcpip_net_protocol tcpip_net_protocols_end[0] __table_end ( tcpip_net_protocols );

struct tcpip_protocol;

/** Registered transport-layer protocols that support TCPIP */
static struct tcpip_protocol tcpip_protocols[0] __table_start ( tcpip_protocols );
static struct tcpip_protocol tcpip_protocols_end[0] __table_end ( tcpip_protocols );

/** Identify TCPIP network-layer protocol
 *
 * @v sa_family	 Network address family
 * @ret tcpip	   Protocol supporting TCPIP, or NULL
 */
static struct tcpip_net_protocol * tcpip_find_protocol ( sa_family_t sa_family ) {
	struct tcpip_net_protocol *tcpip_net;

	for ( tcpip_net = tcpip_net_protocols; 
		tcpip_net < tcpip_net_protocols_end; ++tcpip_net ) {
		if ( tcpip_net->sa_family == sa_family ) {
			return tcpip_net;
		}
	}
	return NULL;
}

/** Identify TCPIP transport-layer protocol
 *
 * @v trans_proto	Transport-layer protocol number, IP_XXX
 * @ret tcpip_protocol	Transport-layer protocol, or NULL
 */
struct tcpip_protocol* find_tcpip_protocol ( uint8_t trans_proto ) {
	struct tcpip_protocol *tcpip;

	for ( tcpip = tcpip_protocols; tcpip <= tcpip_protocols_end; 
		++tcpip ) {
		if ( tcpip->trans_proto == trans_proto ) {
			return tcpip;
		}
	}
	return NULL;
}

/** Process a received packet
 *
 * @v pkb		Packet buffer
 * @v trans_proto	Transport-layer protocol number
 * @v src		Source network-layer address
 * @v dest		Destination network-layer address
 *
 * This function expects a transport-layer segment from the network-layer
 */
void trans_rx ( struct pk_buff *pkb, uint8_t trans_proto, struct in_addr *src,
		struct in_addr *dest ) {
	struct tcpip_protocol *tcpip;

	/* Identify the transport layer protocol */
	for ( tcpip = tcpip_protocols; tcpip <= tcpip_protocols_end; ++tcpip ) {
		if ( tcpip->trans_proto == trans_proto ) {
			DBG ( "Packet sent to %s module", tcpip->name );
			tcpip->rx ( pkb, src, dest );
		}
	}
}

/** Transmit a transport-layer segment
 *
 * @v pkb		Packet buffer
 * @v trans_proto	Transport-layer protocol
 * @v sock		Destination socket address
 * @ret			Status
 */
int trans_tx ( struct pk_buff *pkb, struct tcpip_protocol *tcpip,
	       struct sockaddr *sock ) {

	/* Identify the network layer protocol and send it using xxx_tx() */
	switch ( sock->sa_family ) {
	case AF_INET: /* IPv4 network family */
		return ipv4_tx ( pkb, tcpip, &sock->sin.sin_addr );
	case AF_INET6: /* IPv6 network family */
		return ipv6_tx ( pkb, tcpip, &sock->sin6.sin6_addr );
	}
	DBG ( "Network family %d not supported", sock->sa_family );
	return -EAFNOSUPPORT;
}

/**
 * Calculate internet checksum
 *
 * @v b		Pointer to the data
 * @v len	Length of data to be checksummed
 * @ret result	16 bit internet checksum
 */
uint16_t calc_chksum(void *b, int len) {
	uint16_t *buf = b, result;
	uint16_t sum=0;
	for ( sum = 0; len > 1; len -= 2 ) /* Sum all 16b words */
		sum += *buf++;
	if ( len == 1 )                  /* If any stray bytes, */
		sum += *(unsigned char*)buf;          /* add to sum */
	sum = (sum >> 16) + (sum & 0xffff);    /* Add the carry */
	sum += (sum >> 16);                          /* (again) */
	result = ~sum;             /* Take the one's complement */
	return result;                      /* Return 16b value */
}
