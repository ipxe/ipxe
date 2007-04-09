#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <byteswap.h>
#include <errno.h>
#include <gpxe/tcpip.h>
#include <gpxe/pkbuff.h>
#include <gpxe/netdevice.h>
#include <gpxe/udp.h>

/** @file
 *
 * UDP protocol
 */

struct tcpip_protocol udp_protocol;

/**
 * List of registered UDP connections
 */
static LIST_HEAD ( udp_conns );

/**
 * Bind UDP connection to local port
 *
 * @v conn		UDP connection
 * @v local_port	Local port, in network byte order
 * @ret rc		Return status code
 */
int udp_bind ( struct udp_connection *conn, uint16_t local_port ) {
	struct udp_connection *existing;

	list_for_each_entry ( existing, &udp_conns, list ) {
		if ( existing->local_port == local_port )
			return -EADDRINUSE;
	}
	conn->local_port = local_port;
	return 0;
}

/**
 * Open a local port
 *
 * @v conn		UDP connection
 * @v local_port	Local port, in network byte order, or zero
 * @ret rc		Return status code
 *
 * Opens the UDP connection and binds to a local port.  If no local
 * port is specified, the first available port will be used.
 */
int udp_open ( struct udp_connection *conn, uint16_t local_port ) {
	static uint16_t try_port = 1024;
	int rc;

	/* If no port specified, find the first available port */
	if ( ! local_port ) {
		for ( ; try_port ; try_port++ ) {
			if ( try_port < 1024 )
				continue;
			if ( udp_open ( conn, htons ( try_port ) ) == 0 )
				return 0;
		}
		return -EADDRINUSE;
	}

	/* Attempt bind to local port */
	if ( ( rc = udp_bind ( conn, local_port ) ) != 0 ) {
		DBGC ( conn, "UDP %p could not bind to local port %d: %s\n",
		       conn, local_port, strerror ( rc ) );
		return rc;
	}

	/* Add to UDP connection list */
	list_add ( &conn->list, &udp_conns );
	DBGC ( conn, "UDP %p opened on port %d\n", conn,
	       ntohs ( local_port ) );

	return 0;
}

/**
 * Close a UDP connection
 *
 * @v conn		UDP connection
 */
void udp_close ( struct udp_connection *conn ) {
	list_del ( &conn->list );
	DBGC ( conn, "UDP %p closed\n", conn );
}

/**
 * Allocate packet buffer for UDP
 *
 * @v conn		UDP connection
 * @ret pkb		Packet buffer, or NULL
 */
static struct pk_buff * udp_alloc_pkb ( struct udp_connection *conn ) {
	struct pk_buff *pkb;

	pkb = alloc_pkb ( UDP_MAX_TXPKB );
	if ( ! pkb ) {
		DBGC ( conn, "UDP %p cannot allocate buffer of length %d\n",
		       conn, UDP_MAX_TXPKB );
		return NULL;
	}
	pkb_reserve ( pkb, UDP_MAX_HLEN );
	return pkb;
}

/**
 * User request to send data via a UDP connection
 *
 * @v conn		UDP connection
 *
 * This function allocates buffer space and invokes the function's
 * senddata() callback.  The callback may use the buffer space as
 * temporary storage space.
 */
int udp_senddata ( struct udp_connection *conn ) {
	int rc;

	conn->tx_pkb = udp_alloc_pkb ( conn );
	if ( ! conn->tx_pkb )
		return -ENOMEM;

	rc = conn->udp_op->senddata ( conn, conn->tx_pkb->data, 
				      pkb_tailroom ( conn->tx_pkb ) );
	if ( rc != 0 ) {
		DBGC ( conn, "UDP %p application could not send packet: %s\n",
		       conn, strerror ( rc ) );
	}

	if ( conn->tx_pkb ) {
		free_pkb ( conn->tx_pkb );
		conn->tx_pkb = NULL;
	}

	return rc;
}
		
/**
 * Transmit data via a UDP connection to a specified address
 *
 * @v conn		UDP connection
 * @v peer		Destination address
 * @v netdev		Network device to use if no route found, or NULL
 * @v data		Data to send
 * @v len		Length of data
 * @ret rc		Return status code
 */
int udp_sendto_via ( struct udp_connection *conn, struct sockaddr_tcpip *peer,
		     struct net_device *netdev, const void *data,
		     size_t len ) {
       	struct udp_header *udphdr;
	struct pk_buff *pkb;
	int rc;

	/* Use precreated packet buffer if one is available */
	if ( conn->tx_pkb ) {
		pkb = conn->tx_pkb;
		conn->tx_pkb = NULL;
	} else {
		pkb = udp_alloc_pkb ( conn );
		if ( ! pkb )
			return -ENOMEM;
	}

	/* Avoid overflowing TX buffer */
	if ( len > pkb_tailroom ( pkb ) )
		len = pkb_tailroom ( pkb );

	/* Copy payload */
	memmove ( pkb_put ( pkb, len ), data, len );

	/*
	 * Add the UDP header
	 *
	 * Covert all 16- and 32- bit integers into network btye order before
	 * sending it over the network
	 */
	udphdr = pkb_push ( pkb, sizeof ( *udphdr ) );
	udphdr->dest_port = peer->st_port;
	udphdr->source_port = conn->local_port;
	udphdr->len = htons ( pkb_len ( pkb ) );
	udphdr->chksum = 0;
	udphdr->chksum = tcpip_chksum ( udphdr, sizeof ( *udphdr ) + len );

	/* Dump debugging information */
	DBGC ( conn, "UDP %p TX %d->%d len %zd\n", conn,
	       ntohs ( udphdr->source_port ), ntohs ( udphdr->dest_port ),
	       ntohs ( udphdr->len ) );

	/* Send it to the next layer for processing */
	if ( ( rc = tcpip_tx ( pkb, &udp_protocol, peer, netdev,
			       &udphdr->chksum ) ) != 0 ) {
		DBGC ( conn, "UDP %p could not transmit packet: %s\n",
		       conn, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Transmit data via a UDP connection to a specified address
 *
 * @v conn		UDP connection
 * @v peer		Destination address
 * @v data		Data to send
 * @v len		Length of data
 * @ret rc		Return status code
 */
int udp_sendto ( struct udp_connection *conn, struct sockaddr_tcpip *peer,
		 const void *data, size_t len ) {
	return udp_sendto_via ( conn, peer, NULL, data, len );
}

/**
 * Transmit data via a UDP connection
 *
 * @v conn		UDP connection
 * @v data		Data to send
 * @v len		Length of data
 * @ret rc		Return status code
 */
int udp_send ( struct udp_connection *conn, const void *data, size_t len ) {
	return udp_sendto ( conn, &conn->peer, data, len );
}

/**
 * Identify UDP connection by local port number
 *
 * @v local_port	Local port (in network-endian order)
 * @ret conn		TCP connection, or NULL
 */
static struct udp_connection * udp_demux ( uint16_t local_port ) {
	struct udp_connection *conn;

	list_for_each_entry ( conn, &udp_conns, list ) {
		if ( ( conn->local_port == local_port ) ||
		     ( conn->local_port == 0 ) ) {
			return conn;
		}
	}
	return NULL;
}

/**
 * Process a received packet
 *
 * @v pkb		Packet buffer
 * @v st_src		Partially-filled source address
 * @v st_dest		Partially-filled destination address
 * @v pshdr_csum	Pseudo-header checksum
 * @ret rc		Return status code
 */
static int udp_rx ( struct pk_buff *pkb, struct sockaddr_tcpip *st_src,
		    struct sockaddr_tcpip *st_dest, uint16_t pshdr_csum ) {
	struct udp_header *udphdr = pkb->data;
	struct udp_connection *conn;
	size_t ulen;
	uint16_t csum;
	int rc = 0;

	/* Sanity check packet */
	if ( pkb_len ( pkb ) < sizeof ( *udphdr ) ) {
		DBG ( "UDP packet too short at %d bytes (min %d bytes)\n",
		      pkb_len ( pkb ), sizeof ( *udphdr ) );
		
		rc = -EINVAL;
		goto done;
	}
	ulen = ntohs ( udphdr->len );
	if ( ulen < sizeof ( *udphdr ) ) {
		DBG ( "UDP length too short at %d bytes "
		      "(header is %d bytes)\n", ulen, sizeof ( *udphdr ) );
		rc = -EINVAL;
		goto done;
	}
	if ( ulen > pkb_len ( pkb ) ) {
		DBG ( "UDP length too long at %d bytes (packet is %d bytes)\n",
		      ulen, pkb_len ( pkb ) );
		rc = -EINVAL;
		goto done;
	}
	if ( udphdr->chksum ) {
		csum = tcpip_continue_chksum ( pshdr_csum, pkb->data, ulen );
		if ( csum != 0 ) {
			DBG ( "UDP checksum incorrect (is %04x including "
			      "checksum field, should be 0000)\n", csum );
			rc = -EINVAL;
			goto done;
		}
	}

	/* Parse parameters from header and strip header */
	st_src->st_port = udphdr->source_port;
	st_dest->st_port = udphdr->dest_port;
	conn = udp_demux ( udphdr->dest_port );
	pkb_unput ( pkb, ( pkb_len ( pkb ) - ulen ) );
	pkb_pull ( pkb, sizeof ( *udphdr ) );

	/* Dump debugging information */
	DBGC ( conn, "UDP %p RX %d<-%d len %zd\n", conn,
	       ntohs ( udphdr->dest_port ), ntohs ( udphdr->source_port ),
	       ulen );

	/* Ignore if no matching connection found */
	if ( ! conn ) {
		DBG ( "No UDP connection listening on port %d\n",
		      ntohs ( udphdr->dest_port ) );
		rc = -ENOTCONN;
		goto done;
	}

	/* Pass data to application */
	if ( conn->udp_op->newdata ) {
		rc = conn->udp_op->newdata ( conn, pkb->data, pkb_len ( pkb ),
				     st_src, st_dest );
		if ( rc != 0 ) {
			DBGC ( conn, "UDP %p application rejected packet: %s\n",
			       conn, strerror ( rc ) );
		}
	} else {
		DBGC ( conn, "UDP %p application has no newdata handler for " \
			"incoming packet\n", conn );
	}

 done:
	free_pkb ( pkb );
	return rc;
}

struct tcpip_protocol udp_protocol __tcpip_protocol = {
	.name = "UDP",
	.rx = udp_rx,
	.tcpip_proto = IP_UDP,
};
