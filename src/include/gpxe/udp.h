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
#include <gpxe/in.h>

struct udp_connection;

/**
 * UDP operations
 *
 */
struct udp_operations {
	/**
	 * New data received
	 *
	 * @v conn	UDP connection
	 * @v data	Data
	 * @v len	Length of data
	 */
	void ( * newdata ) ( struct udp_connection *conn,
			     void *data, size_t len );
};

/**
 * A UDP connection
 *
 */
struct udp_connection {
	/** Address of the remote end of the connection */
	struct sockaddr_in sin;
	/** Operations table for this connection */
	struct udp_operations *udp_op;
};

extern void udp_connect ( struct udp_connection *conn );
extern void udp_send ( struct udp_connection *conn, const void *data,
		       size_t len );
extern void udp_close ( struct udp_connection *conn );

#endif /* _GPXE_UDP_H */
