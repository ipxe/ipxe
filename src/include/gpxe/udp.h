#ifndef _GPXE_UDP_H
#define _GPXE_UDP_H

/** @file
 *
 * UDP protocol
 *
 * This file defines the gPXE UDP API.
 *
 */

#include <stddef.h>
#include <gpxe/iobuf.h>
#include <gpxe/tcpip.h>
#include <gpxe/if_ether.h>

struct net_device;

/**
 * UDP constants
 */

#define UDP_MAX_HLEN	72
#define UDP_MAX_TXIOB	ETH_MAX_MTU
#define UDP_MIN_TXIOB	ETH_ZLEN

typedef uint16_t port_t;

/**
 * A UDP header
 */
struct udp_header {
	port_t source_port;
	port_t dest_port;
	uint16_t len;
	uint16_t chksum;
};

struct udp_connection;

/**
 * UDP operations
 *
 */
struct udp_operations {
	
	/**
	 * Transmit data
	 *
	 * @v conn	UDP connection
	 * @v buf	Temporary data buffer
	 * @v len	Length of temporary data buffer
	 * @ret rc	Return status code
	 *
	 * The application may use the temporary data buffer to
	 * construct the data to be sent.  Note that merely filling
	 * the buffer will do nothing; the application must call
	 * udp_send() in order to actually transmit the data.  Use of
	 * the buffer is not compulsory; the application may call
	 * udp_send() on any block of data.
	 */
	int ( * senddata ) ( struct udp_connection *conn, void *buf,
			     size_t len );
	/**
	 * New data received
	 *
	 * @v conn	UDP connection
	 * @v data	Data
	 * @v len	Length of data
	 * @v st_src	Source address
	 * @v st_dest	Destination address
	 * @ret rc	Return status code
	 */
	int ( * newdata ) ( struct udp_connection *conn, void *data,
			    size_t len, struct sockaddr_tcpip *st_src,
			    struct sockaddr_tcpip *st_dest );
};

/**
 * A UDP connection
 *
 */
struct udp_connection {
       /** Address of the remote end of the connection */
	struct sockaddr_tcpip peer;
	/** Local port on which the connection receives packets */
	port_t local_port;
	/** Transmit buffer */
	struct io_buffer *tx_iob;
	/** List of registered connections */
	struct list_head list;
	/** Operations table for this connection */
	struct udp_operations *udp_op;
};

/*
 * Functions provided to the application layer
 */

/**
 * Bind UDP connection to all local ports
 *
 * @v conn		UDP connection
 *
 * A promiscuous UDP connection will receive packets with any
 * destination UDP port.  This is required in order to support the PXE
 * UDP API.
 *
 * If the promiscuous connection is not the only UDP connection, the
 * behaviour is undefined.
 */
static inline void udp_bind_promisc ( struct udp_connection *conn ) {
	conn->local_port = 0;
}

/**
 * Connect UDP connection to remote host and port
 *
 * @v conn		UDP connection
 * @v peer		Destination socket address
 *
 * This function sets the default address for transmitted packets,
 * i.e. the address used when udp_send() is called rather than
 * udp_sendto().
 */
static inline void udp_connect ( struct udp_connection *conn,
				 struct sockaddr_tcpip *peer ) {
	memcpy ( &conn->peer, peer, sizeof ( conn->peer ) );
}

/**
 * Connect UDP connection to remote port
 *
 * @v conn		UDP connection
 * @v port		Destination port
 *
 * This function sets only the port part of the default address for
 * transmitted packets.
 */
static inline void udp_connect_port ( struct udp_connection *conn,
				      uint16_t port ) {
	conn->peer.st_port = port;
}

/**
 * Get default address for transmitted packets
 *
 * @v conn		UDP connection
 * @ret peer		Default destination socket address
 */
static inline struct sockaddr_tcpip *
udp_peer ( struct udp_connection *conn ) {
	return &conn->peer;
}

extern int udp_bind ( struct udp_connection *conn, uint16_t local_port );
extern int udp_open ( struct udp_connection *conn, uint16_t local_port );
extern void udp_close ( struct udp_connection *conn );

extern int udp_senddata ( struct udp_connection *conn );
extern int udp_send ( struct udp_connection *conn,
		      const void *data, size_t len );
extern int udp_sendto ( struct udp_connection *conn,
			struct sockaddr_tcpip *peer,
			const void *data, size_t len );
int udp_sendto_via ( struct udp_connection *conn, struct sockaddr_tcpip *peer,
		     struct net_device *netdev, const void *data,
		     size_t len );

#endif /* _GPXE_UDP_H */
