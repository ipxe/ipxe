#ifndef _GPXE_INTERFACE_H
#define _GPXE_INTERFACE_H

/** @file
 *
 * Transport-network layer interface
 *
 */

#include <stdint.h>
#include <gpxe/in.h>
#include <gpxe/tables.h>

struct pk_buff;
struct net_protocol;
struct trans_protocol;
struct tcpip_net_protocol;

/** 
 * A transport-layer protocol
 */
struct trans_protocol {
	/** Protocol name */
	const char *name;
       	/**
         * Process received packet
         *
         * @v pkb       Packet buffer
         * @v netdev    Network device
         * @v ll_source Link-layer source address
         *
         * This method takes ownership of the packet buffer.
         */
        void ( * rx ) ( struct pk_buff *pkb, struct in_addr *src_net_addr, struct in_addr *dest_net_addr );
        /** 
	 * Transport-layer protocol number
	 *
	 * This is a constant of the type IP_XXX
         */
        uint8_t trans_proto;
};

/**
 * A TCPIP supporting protocol
 */
struct tcpip_net_protocol {
	/** Network protocol */
	struct net_protocol *net_protocol;
	/** Network address family */
	sa_family_t sa_family;
	/** Complete transport-layer checksum calculation
	 *
	 * @v pkb		Packet buffer
	 * @v trans_proto	Transport-layer protocol number
	 *
	 * This function expects a network-layer datagram in its packet with the protocol field in the
	 * IP header to be filled up. It constructs a psuedo-header using the information provided in
	 * the IP header and computes the checksum over the pseudo-header. The checksum offset in the
	 * transport layer header can be determined without the need of an offset value as
	 * 
	 * void *csum_offset = pkb->data + NET_HLEN + csum_offset ( trans_proto );
	 * 
	 * where,
	 * csum_offset ( IP_TCP ) = 16
	 * csum_offset ( IP_UDP ) = 6
	 */
	void ( * tx_csum ) ( struct pk_buff *pkb );
};

/**
 * Register a transport-layer protocol
 *
 * @v protocol          Transport-layer protocol
 */
#define TRANS_PROTOCOL( protocol ) \
        struct trans_protocol protocol __table ( trans_protocols, 01 )

#define TCPIP_NET_PROTOCOL( protocol ) \
        struct tcpip_net_protocol protocol __table ( tcpip_net_protocols, 01 )

extern void trans_rx ( struct pk_buff *pkb, uint8_t trans_proto, struct in_addr *src, struct in_addr *dest );
extern int trans_tx ( struct pk_buff *pkb, uint8_t trans_proto, struct sockaddr *dest );

extern uint16_t calc_chksum ( void *data, size_t len );

/** Do we need these functions? -Nikhil, 24-6-06 */
extern struct trans_protocol * find_trans_protocol ( uint8_t trans_proto );
extern struct tcpip_net_protocol * find_tcpip_net_protocol ( sa_family_t sa_family );

#endif /* _GPXE_INTERFACE_H */
