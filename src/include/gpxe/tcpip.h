#ifndef _GPXE_TCPIP_H
#define _GPXE_TCPIP_H

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
struct tcpip_protocol;
struct tcpip_net_protocol;

/** 
 * A transport-layer protocol of the TCPIP stack (eg. UDP, TCP, etc)
 */
struct tcpip_protocol {
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
	/**
	 * Checksum offset
	 *
	 * A negative number indicates that the protocol does not require
	 * checksumming to be performed by the network layer. A positive number is
	 * the offset of the checksum field in the transport-layer header.
	 */
	int csum_offset;
};

/**
 * A TCPIP supporting network-layer protocol
 */
struct tcpip_net_protocol {
	/** Network protocol */
	struct net_protocol *net_protocol;
	/** Network address family */
	sa_family_t sa_family;
	/** Complete transport-layer checksum calculation
	 *
	 * @v pkb		Packet buffer
	 * @v tcpip		Transport-layer protocol
	 *
	 */
	void ( * tx_csum ) ( struct pk_buff *pkb,
			     struct tcpip_protocol *tcpip );
};

/**
 * Register a transport-layer protocol
 *
 * @v protocol          Transport-layer protocol
 */
#define TCPIP_PROTOCOL( protocol ) \
        struct tcpip_protocol protocol __table ( tcpip_protocols, 01 )

#define TCPIP_NET_PROTOCOL( protocol ) \
        struct tcpip_net_protocol protocol __table ( tcpip_net_protocols, 01 )

extern void tcpip_rx ( struct pk_buff *pkb, uint8_t trans_proto, 
		       struct in_addr *src, struct in_addr *dest );

extern int tcpip_tx ( struct pk_buff *pkb, struct tcpip_protocol *tcpip, 
		      struct sockaddr *dest );

extern unsigned int tcpip_continue_chksum ( unsigned int partial,
					    const void *data, size_t len );
extern unsigned int tcpip_chksum ( const void *data, size_t len );

#endif /* _GPXE_TCPIP_H */
