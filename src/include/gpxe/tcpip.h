#ifndef _GPXE_TCPIP_H
#define _GPXE_TCPIP_H

/** @file
 *
 * Transport-network layer interface
 *
 */

#include <stdint.h>
#include <gpxe/socket.h>
#include <gpxe/in.h>
#include <gpxe/tables.h>

struct pk_buff;

#define SA_TCPIP_LEN 32

/**
 * TCP/IP socket address
 *
 * This contains the fields common to socket addresses for all TCP/IP
 * address families.
 */
struct sockaddr_tcpip {
	/** Socket address family (part of struct @c sockaddr) */
	sa_family_t st_family;
	/** TCP/IP port */
	uint16_t st_port;
	/** Padding
	 *
	 * This ensures that a struct @c sockaddr_tcpip is large
	 * enough to hold a socket address for any TCP/IP address
	 * family.
	 */
	char pad[SA_TCPIP_LEN - sizeof ( sa_family_t ) - sizeof ( uint16_t )];
};

/** 
 * A transport-layer protocol of the TCP/IP stack (eg. UDP, TCP, etc)
 */
struct tcpip_protocol {
	/** Protocol name */
	const char *name;
       	/**
         * Process received packet
         *
         * @v pkb	Packet buffer
	 * @v st_src	Partially-filled source address
	 * @v st_dest	Partially-filled destination address
	 * @ret rc	Return status code
         *
         * This method takes ownership of the packet buffer.
         */
        int ( * rx ) ( struct pk_buff *pkb, struct sockaddr_tcpip *st_src,
		       struct sockaddr_tcpip *st_dest );
        /** 
	 * Transport-layer protocol number
	 *
	 * This is a constant of the type IP_XXX
         */
        uint8_t tcpip_proto;
	/**
	 * Checksum offset
	 *
	 * A negative number indicates that the protocol does not
	 * require checksumming to be performed by the network layer.
	 * A positive number is the offset of the checksum field in
	 * the transport-layer header.
	 */
	int csum_offset;
};

/**
 * A network-layer protocol of the TCP/IP stack (eg. IPV4, IPv6, etc)
 */
struct tcpip_net_protocol {
	/** Protocol name */
	const char *name;
	/** Network address family */
	sa_family_t sa_family;
	/**
	 * Transmit packet
	 *
	 * @v pkb		Packet buffer
	 * @v tcpip_protocol	Transport-layer protocol
	 * @v st_dest		Destination address
	 * @ret rc		Return status code
	 *
	 * This function takes ownership of the packet buffer.
	 */
	int ( * tx ) ( struct pk_buff *pkb,
		       struct tcpip_protocol *tcpip_protocol,
		       struct sockaddr_tcpip *st_dest );
};

/** Declare a TCP/IP transport-layer protocol */
#define	__tcpip_protocol __table ( tcpip_protocols, 01 )

/** Declare a TCP/IP network-layer protocol */
#define	__tcpip_net_protocol __table ( tcpip_net_protocols, 01 )

extern int tcpip_rx ( struct pk_buff *pkb, uint8_t tcpip_proto,
		      struct sockaddr_tcpip *st_src,
		      struct sockaddr_tcpip *st_dest );
extern int tcpip_tx ( struct pk_buff *pkb, struct tcpip_protocol *tcpip, 
		      struct sockaddr_tcpip *st_dest );
extern unsigned int tcpip_continue_chksum ( unsigned int partial,
					    const void *data, size_t len );
extern unsigned int tcpip_chksum ( const void *data, size_t len );

#endif /* _GPXE_TCPIP_H */
