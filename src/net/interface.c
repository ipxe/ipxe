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
#include <gpxe/interface.h>

/** @file
 *
 * Transport-network layer interface
 *
 * This file contains functions and utilities for the transport-network layer interface
 */

/** Registered protocols that support TCPIP */
static struct tcpip_net_protocol tcpip_net_protocols[0] __table_start ( tcpip_net_protocols );
static struct tcpip_net_protocol tcpip_net_protocols_end[0] __table_end ( tcpip_net_protocols );

struct trans_protocol;

/** Registered transport-layer protocols */
static struct trans_protocol trans_protocols[0] __table_start ( trans_protocols );
static struct trans_protocol trans_protocols_end[0] __table_end ( trans_protocols );

/** Identify TCPIP net protocol
 *
 * @v sa_family         Network address family
 * @ret tcpip           Protocol supporting TCPIP, or NULL
 */
static struct tcpip_net_protocol * tcpip_find_protocol ( sa_family_t sa_family ) {
        struct tcpip_net_protocol *tcpip;

        for ( tcpip = tcpip_net_protocols; tcpip < tcpip_net_protocols_end; tcpip++ ) {
                if ( tcpip->sa_family == sa_family ) {
                        return tcpip;
                }
        }
        return NULL;
}

/** Identify transport-layer protocol
 *
 * @v trans_proto	Transport-layer protocol number, IP_XXX
 * @ret trans_protocol	Transport-layer protocol, or NULL
 */
struct trans_protocol* find_trans_protocol ( uint8_t trans_proto ) {
        struct trans_protocol *trans_protocol;

        for ( trans_protocol = trans_protocols; trans_protocol <= trans_protocols_end; ++trans_protocol ) {
                if ( trans_protocol->trans_proto == trans_proto ) {
                        return trans_protocol;
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
void trans_rx ( struct pk_buff *pkb, uint8_t trans_proto, struct in_addr *src, struct in_addr *dest ) {
        struct trans_protocol *trans_protocol;

        /* Identify the transport layer protocol */
        for ( trans_protocol = trans_protocols; trans_protocol <= trans_protocols_end; ++trans_protocol ) {
                if ( trans_protocol->trans_proto == trans_proto ) {
			DBG ( "Packet sent to %s module", trans_protocol->name );
                        trans_protocol->rx ( pkb, src, dest );
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
int trans_tx ( struct pk_buff *pkb, uint8_t trans_proto, struct sockaddr *sock ) {

        /* Identify the network layer protocol and send it using xxx_tx() */
        switch ( sock->sa_family ) {
        case AF_INET: /* IPv4 network family */
                return ipv4_tx ( pkb, trans_proto, &sock->sin.sin_addr );
        case AF_INET6: /* IPv6 network family */
                return ipv6_tx ( pkb, trans_proto, &sock->sin6.sin6_addr );
        default:
                DBG ( "Network family %d not supported", sock->sa_family );
        }
        return -EPROTONOSUPPORT;
}

/**
 * Calculate internet checksum
 *
 * @v data      Pointer to the data
 * @v len       Length of data to be checksummed
 * @ret chksum  16 bit internet checksum
 *
 * This function calculates the internet checksum (refer RFC1071) for len bytes beginning at the location data
 */
uint16_t calc_chksum ( void *data, size_t len ) {
        register long sum = 0;
        uint16_t checksum;
        unsigned short *temp;
        while ( len > 1 ) {
                temp = (unsigned short*) data++;
                sum += *temp;
                len -= 2;
        }
        if ( len > 0 ) {
                sum += *(unsigned char *)data;
        }
        while ( sum >> 16 ) {
                sum = ( sum & 0xffff ) + ( sum >> 16 );
        }
        checksum = ~sum;
        return checksum;
}


