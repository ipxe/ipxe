#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <gpxe/pkbuff.h>
#include <gpxe/tables.h>
#include <gpxe/tcpip.h>

/** @file
 *
 * Transport-network layer interface
 *
 * This file contains functions and utilities for the
 * TCP/IP transport-network layer interface
 */

/** Registered network-layer protocols that support TCP/IP */
static struct tcpip_net_protocol
tcpip_net_protocols[0] __table_start ( tcpip_net_protocols );
static struct tcpip_net_protocol
tcpip_net_protocols_end[0] __table_end ( tcpip_net_protocols );

/** Registered transport-layer protocols that support TCP/IP */
static struct tcpip_protocol
tcpip_protocols[0]__table_start ( tcpip_protocols );
static struct tcpip_protocol
tcpip_protocols_end[0] __table_end ( tcpip_protocols );

/** Process a received TCP/IP packet
 *
 * @v pkb		Packet buffer
 * @v tcpip_proto	Transport-layer protocol number
 * @v st_src		Partially-filled source address
 * @v st_dest		Partially-filled destination address
 * @ret rc		Return status code
 *
 * This function expects a transport-layer segment from the network
 * layer.  The network layer should fill in as much as it can of the
 * source and destination addresses (i.e. it should fill in the
 * address family and the network-layer addresses, but leave the ports
 * and the rest of the structures as zero).
 */
int tcpip_rx ( struct pk_buff *pkb, uint8_t tcpip_proto, 
	       struct sockaddr_tcpip *st_src,
	       struct sockaddr_tcpip *st_dest ) {
	struct tcpip_protocol *tcpip;

	/* Hand off packet to the appropriate transport-layer protocol */
	for ( tcpip = tcpip_protocols; tcpip < tcpip_protocols_end; tcpip++ ) {
		if ( tcpip->tcpip_proto == tcpip_proto ) {
			DBG ( "TCP/IP received %s packet\n", tcpip->name );
			return tcpip->rx ( pkb, st_src, st_dest );
		}
	}

	DBG ( "Unrecognised TCP/IP protocol %d\n", tcpip_proto );
	return -EPROTONOSUPPORT;
}

/** Transmit a TCP/IP packet
 *
 * @v pkb		Packet buffer
 * @v tcpip_protocol	Transport-layer protocol
 * @v st_dest		Destination address
 * @ret rc		Return status code
 */
int tcpip_tx ( struct pk_buff *pkb, struct tcpip_protocol *tcpip_protocol,
	       struct sockaddr_tcpip *st_dest ) {
	struct tcpip_net_protocol *tcpip_net;

	/* Hand off packet to the appropriate network-layer protocol */
	for ( tcpip_net = tcpip_net_protocols ;
	      tcpip_net < tcpip_net_protocols_end ; tcpip_net++ ) {
		if ( tcpip_net->sa_family == st_dest->st_family ) {
			DBG ( "TCP/IP sending %s packet\n", tcpip_net->name );
			return tcpip_net->tx ( pkb, tcpip_protocol, st_dest );
		}
	}
	
	DBG ( "Unrecognised TCP/IP address family %d\n", st_dest->st_family );
	return -EAFNOSUPPORT;
}

/**
 * Calculate continued TCP/IP checkum
 *
 * @v partial		Checksum of already-summed data, in network byte order
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret cksum		Updated checksum, in network byte order
 *
 * Calculates a TCP/IP-style 16-bit checksum over the data block.  The
 * checksum is returned in network byte order.
 *
 * This function may be used to add new data to an existing checksum.
 * The function assumes that both the old data and the new data start
 * on even byte offsets; if this is not the case then you will need to
 * byte-swap either the input partial checksum, the output checksum,
 * or both.  Deciding which to swap is left as an exercise for the
 * interested reader.
 */
unsigned int tcpip_continue_chksum ( unsigned int partial, const void *data,
				     size_t len ) {
	unsigned int cksum = ( ( ~partial ) & 0xffff );
	unsigned int value;
	unsigned int i;
	
	for ( i = 0 ; i < len ; i++ ) {
		value = * ( ( uint8_t * ) data + i );
		if ( i & 1 ) {
			/* Odd bytes: swap on little-endian systems */
			value = be16_to_cpu ( value );
		} else {
			/* Even bytes: swap on big-endian systems */
			value = le16_to_cpu ( value );
		}
		cksum += value;
		if ( cksum > 0xffff )
			cksum -= 0xffff;
	}
	
	return ( ( ~cksum ) & 0xffff );
}

/**
 * Calculate TCP/IP checkum
 *
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret cksum		Checksum, in network byte order
 *
 * Calculates a TCP/IP-style 16-bit checksum over the data block.  The
 * checksum is returned in network byte order.
 */
unsigned int tcpip_chksum ( const void *data, size_t len ) {
	return tcpip_continue_chksum ( 0xffff, data, len );
}
