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
void udp_bind_promisc ( struct udp_connection *conn ) {
	conn->local_port = 0;
}

/**
 * Connect UDP connection to remote host and port
 *
 * @v conn		UDP connection
 * @v peer		Destination socket address
 *
 * This function stores the socket address within the connection
 */
void udp_connect ( struct udp_connection *conn, struct sockaddr_tcpip *peer ) {
	memcpy ( &conn->peer, peer, sizeof ( conn->peer ) );
}

/**
 * Connect UDP connection to all remote hosts and ports
 *
 * @v conn		UDP connection
 *
 * This undoes the effect of a call to udp_connect(), i.e. allows the
 * connection to receive packets from all remote hosts and ports.
 */
void udp_connect_promisc ( struct udp_connection *conn ) {
	memset ( &conn->peer, 0, sizeof ( conn->peer ) );
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
	if ( ( rc = udp_bind ( conn, local_port ) ) != 0 )
		return rc;

	/* Add to UDP connection list */
	list_add ( &conn->list, &udp_conns );
	DBG ( "UDP opened %p on port %d\n", conn, ntohs ( local_port ) );

	return 0;
}

/**
 * Close a UDP connection
 *
 * @v conn		UDP connection
 */
void udp_close ( struct udp_connection *conn ) {
	list_del ( &conn->list );
	DBG ( "UDP closed %p\n", conn );
}

/**
 * User request to send data via a UDP connection
 *
 * @v conn		UDP connection
 *
 * This function allocates buffer space and invokes the function's senddata()
 * callback. The callback may use the buffer space
 */
int udp_senddata ( struct udp_connection *conn ) {
	conn->tx_pkb = alloc_pkb ( UDP_MAX_TXPKB );
	if ( conn->tx_pkb == NULL ) {
		DBG ( "UDP %p cannot allocate packet buffer of length %d\n",
		      conn, UDP_MAX_TXPKB );
		return -ENOMEM;
	}
	pkb_reserve ( conn->tx_pkb, UDP_MAX_HLEN );
	conn->udp_op->senddata ( conn, conn->tx_pkb->data, 
				 pkb_available ( conn->tx_pkb ) );
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
 *
 * This function fills up the UDP headers and sends the data.  It may
 * be called only from within the context of an application's
 * senddata() method; if the application wishes to send data it must
 * call udp_senddata() and wait for its senddata() method to be
 * called.
 */
int udp_sendto ( struct udp_connection *conn, struct sockaddr_tcpip *peer,
		 const void *data, size_t len ) {
       	struct udp_header *udphdr;

	/* Avoid overflowing TX buffer */
	if ( len > pkb_available ( conn->tx_pkb ) )
		len = pkb_available ( conn->tx_pkb );

	/* Copy payload */
	memmove ( pkb_put ( conn->tx_pkb, len ), data, len );

	/*
	 * Add the UDP header
	 *
	 * Covert all 16- and 32- bit integers into network btye order before
	 * sending it over the network
	 */
	udphdr = pkb_push ( conn->tx_pkb, sizeof ( *udphdr ) );
	udphdr->dest_port = peer->st_port;
	udphdr->source_port = conn->local_port;
	udphdr->len = htons ( pkb_len ( conn->tx_pkb ) );
	udphdr->chksum = 0;
	udphdr->chksum = tcpip_chksum ( udphdr, sizeof ( *udphdr ) + len );

	/* Dump debugging information */
	DBG ( "UDP %p transmitting %p+%#zx len %#x src %d dest %d "
	      "chksum %#04x\n", conn, conn->tx_pkb->data,
	      pkb_len ( conn->tx_pkb ), ntohs ( udphdr->len ),
	      ntohs ( udphdr->source_port ), ntohs ( udphdr->dest_port ),
	      ntohs ( udphdr->chksum ) );

	/* Send it to the next layer for processing */
	return tcpip_tx ( conn->tx_pkb, &udp_protocol, peer );
}

/**
 * Transmit data via a UDP connection
 *
 * @v conn		UDP connection
 * @v data		Data to send
 * @v len		Length of data
 * @ret rc		Return status code
 *
 * This function fills up the UDP headers and sends the data.  It may
 * be called only from within the context of an application's
 * senddata() method; if the application wishes to send data it must
 * call udp_senddata() and wait for its senddata() method to be
 * called.
 */
int udp_send ( struct udp_connection *conn, const void *data, size_t len ) {
	return udp_sendto ( conn, &conn->peer, data, len );
}

/**
 * Process a received packet
 *
 * @v pkb		Packet buffer
 * @v st_src		Partially-filled source address
 * @v st_dest		Partially-filled destination address
 * @ret rc		Return status code
 */
static int udp_rx ( struct pk_buff *pkb, struct sockaddr_tcpip *st_src,
		    struct sockaddr_tcpip *st_dest ) {
	struct udp_header *udphdr = pkb->data;
	struct udp_connection *conn;
	unsigned int ulen;
	uint16_t chksum;

	/* Sanity check */
	if ( pkb_len ( pkb ) < sizeof ( *udphdr ) ) {
		DBG ( "UDP received underlength packet %p+%#zx\n",
		      pkb->data, pkb_len ( pkb ) );
		return -EINVAL;
	}

	/* Dump debugging information */
	DBG ( "UDP received %p+%#zx len %#x src %d dest %d chksum %#04x\n",
	      pkb->data, pkb_len ( pkb ), ntohs ( udphdr->len ),
	      ntohs ( udphdr->source_port ), ntohs ( udphdr->dest_port ),
	      ntohs ( udphdr->chksum ) );

	/* Check length and trim any excess */
	ulen = ntohs ( udphdr->len );
	if ( ulen > pkb_len ( pkb ) ) {
		DBG ( "UDP received truncated packet %p+%#zx\n",
		      pkb->data, pkb_len ( pkb ) );
		return -EINVAL;
	}
	pkb_unput ( pkb, ( pkb_len ( pkb ) - ulen ) );

	/* Verify the checksum */
#warning "Don't we need to take the pseudo-header into account here?"
#if 0
	chksum = tcpip_chksum ( pkb->data, pkb_len ( pkb ) );
	if ( chksum != 0xffff ) {
		DBG ( "Bad checksum %#x\n", chksum );
		return -EINVAL;
	}
#endif

	/* Complete the socket addresses */
	st_src->st_port = udphdr->source_port;
	st_dest->st_port = udphdr->dest_port;

	/* Demux the connection */
	list_for_each_entry ( conn, &udp_conns, list ) {
		if ( conn->local_port &&
		     ( conn->local_port != udphdr->dest_port ) ) {
			/* Bound to local port and local port doesn't match */
			continue;
		}
		if ( conn->peer.st_family &&
		     ( memcmp ( &conn->peer, st_src,
				sizeof ( conn->peer ) ) != 0 ) ) {
			/* Connected to remote port and remote port
			 * doesn't match
			 */
			continue;
		}
		
		/* Strip off the UDP header */
		pkb_pull ( pkb, sizeof ( *udphdr ) );

		DBG ( "UDP delivering to %p\n", conn );
		
		/* Call the application's callback */
		return conn->udp_op->newdata ( conn, pkb->data, pkb_len( pkb ),
					       st_src, st_dest );
	}

	DBG ( "No UDP connection listening on port %d\n",
	      ntohs ( udphdr->dest_port ) );
	return 0;
}

struct tcpip_protocol udp_protocol  = {
	.name = "UDP",
	.rx = udp_rx,
	.tcpip_proto = IP_UDP,
	.csum_offset = 6,
};

TCPIP_PROTOCOL ( udp_protocol );
