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
#include <gpxe/pkbuff.h>
#include <gpxe/tcpip.h>
#include <gpxe/if_ether.h>

/**
 * UDP constants
 */

#define UDP_MAX_HLEN	72
#define UDP_MAX_TXPKB	ETH_MAX_MTU
#define UDP_MIN_TXPKB	ETH_ZLEN

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
	 *
	 * The application may use the temporary data buffer to
	 * construct the data to be sent.  Note that merely filling
	 * the buffer will do nothing; the application must call
	 * udp_send() in order to actually transmit the data.  Use of
	 * the buffer is not compulsory; the application may call
	 * udp_send() on any block of data.
	 */
	void ( * senddata ) ( struct udp_connection *conn, void *buf,
			      size_t len );
	/**
	 * New data received
	 *
	 * @v conn	UDP connection
	 * @v data	Data
	 * @v len	Length of data
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
	struct pk_buff *tx_pkb;
	/** List of registered connections */
	struct list_head list;
	/** Operations table for this connection */
	struct udp_operations *udp_op;
};

/*
 * Functions provided to the application layer
 */

extern int udp_bind ( struct udp_connection *conn, uint16_t local_port );
extern void udp_connect ( struct udp_connection *conn,
			  struct sockaddr_tcpip *peer );
extern int udp_open ( struct udp_connection *conn, uint16_t local_port );
extern void udp_close ( struct udp_connection *conn );

extern int udp_senddata ( struct udp_connection *conn );
extern int udp_send ( struct udp_connection *conn,
		      const void *data, size_t len );
extern int udp_sendto ( struct udp_connection *conn,
			struct sockaddr_tcpip *peer,
			const void *data, size_t len );

#endif /* _GPXE_UDP_H */
