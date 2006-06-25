#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <byteswap.h>
#include <latch.h>
#include <errno.h>
#include <gpxe/in.h>
#include <gpxe/ip.h>
#include <gpxe/udp.h>
#include <gpxe/init.h>
#include <gpxe/pkbuff.h>
#include <gpxe/netdevice.h>
#include <gpxe/interface.h>

/** @file
 *
 * UDP protocol
 */

void copy_sockaddr ( struct sockaddr *source, struct sockaddr *dest );

/**
 * Open a UDP connection
 *
 * @v conn      UDP connection
 * @v peer      Destination socket address
 *
 * This function stores the socket address within the connection
 */
void udp_connect ( struct udp_connection *conn, struct sockaddr *peer ) {
        copy_sockaddr ( peer, &conn->sin );
        /**
         * Not sure if this should add the connection to udp_conns; If it does, uncomment the following code
         */
//	list_add ( &conn->list, &udp_conns );
}

/**
 * Initialize a UDP connection
 *
 * @v conn      UDP connection
 * @v udp_op	UDP operations
 */
void udp_init ( struct udp_connection *conn, struct udp_operations *udp_op ) {
        conn->local_port = 0;
        conn->tx_pkb = NULL;
        if ( udp_op != NULL ) {
                conn->udp_op = udp_op;
        }
}

/**
 * Allocate space to the UDP buffer
 *
 * @v conn      UDP connection
 * @v len       Length to allocate
 * @ret rc      Status
 *
 * Allocate "len" amount of space in the transmit buffer
 */
int udp_buf_alloc ( struct udp_connection *conn, size_t len ) {
        if ( conn->tx_pkb != NULL ) {
                free_pkb ( conn->tx_pkb );
                conn->tx_pkb = NULL;
        }
        conn->tx_pkb = alloc_pkb ( len < UDP_MIN_TXPKB ? UDP_MIN_TXPKB : len );
        return !conn ? -ENOMEM : 0;
}

/**
 * Send data via a UDP connection
 *
 * @v conn      UDP connection
 * @v data      Data to send
 * @v len       Length of data
 *
 * This function fills up the UDP headers and sends the data. Discover the network protocol to
 * use through the sa_family field in the destination socket address.
 */
int udp_send ( struct udp_connection *conn, const void *data, size_t len ) {
       	struct udp_header *udphdr;              /* UDP header */
        struct sockaddr *sock = &conn->sin;     /* Destination sockaddr */
        int rc;

        /* Copy data into packet buffer if necessary */
        if ( data != conn->tx_pkb->data ) {
                /* Allocate a buffer */
                if ( ( rc = udp_buf_alloc ( conn, len + UDP_MAX_HLEN ) ) != 0 ) {
                        DBG ( "Error allocating buffer" );
                        return rc;
                }

                /* Reserve space for the headers and copy contents */
                pkb_reserve ( conn->tx_pkb, UDP_MAX_HLEN );
                memcpy ( pkb_put ( conn->tx_pkb, len ), data, len );
        }

        /* Add the UDP header */
        udphdr = pkb_push ( conn->tx_pkb, UDP_HLEN );
        if ( sock->sa_family == AF_INET )
                udphdr->dest_port = sock->sin.sin_port;
        else
                udphdr->dest_port = sock->sin6.sin6_port;
        udphdr->source_port = conn->local_port;
        udphdr->len = htons ( pkb_len ( conn->tx_pkb ) );
        udphdr->chksum = calc_chksum ( udphdr, UDP_HLEN );

        /* Print UDP header for debugging */
        DBG ( "UDP header at %#x + %d\n", udphdr, UDP_HDR_LEN  );
        DBG ( "\tSource Port = %d\n", udphdr->source_port );
        DBG ( "\tDestination Port = %d\n", udphdr->dest_port );
        DBG ( "\tLength = %d\n", udphdr->len );
        DBG ( "\tChecksum = %d\n", udphdr->chksum );
        DBG ( "\tChecksum located at %#x\n", &udphdr->chksum );

	return trans_tx ( conn->tx_pkb, IP_UDP, sock );
}

/**
 * Send data to a specified address
 *
 * @v conn      UDP connection
 * @v peer      Destination address
 * @v data      Data to send
 * @v len       Length of data
 */
int udp_sendto ( struct udp_connection *conn, struct sockaddr *peer,
                        const void *data, size_t len ) {
        struct sockaddr tempsock;
        copy_sockaddr ( &conn->sin, &tempsock );
        copy_sockaddr ( peer, &conn->sin );
        int rc = udp_send ( conn, data, len );
        copy_sockaddr ( &tempsock, &conn->sin );
        return rc;
}

/**
 * Close a UDP connection
 *
 * @v conn      UDP connection
 */
void udp_close ( struct udp_connection *conn ) {
        list_del ( &conn->list );
}

/**
 * Open a local port
 *
 * @v conn		UDP connection
 * @v local_port	Local port on which to open connection
 *
 * This does not support the 0 port option correctly yet
 */
int udp_open ( struct udp_connection *conn, uint16_t local_port ) {
        struct udp_connection *connr;
        uint16_t min_port = 0xffff;
        list_for_each_entry ( connr, &udp_conns, list ) {
                if ( connr->local_port == local_port ) {
                        return -EISCONN; /* CHECK THE ERROR NUMBER */
                }
                if ( min_port > connr->local_port ) {
                        min_port = connr->local_port;
                }
        }
        conn->local_port = local_port == 0 ? min_port > 1024 ? 1024 : min_port + 1 : local_port; // FAULTY!!!
        list_add ( &conn->list, &udp_conns );
        return 0;
}

/**
 * Process a received packet
 *
 * @v pkb               Packet buffer
 * @v src_net_addr      Source network address
 * @v dest_net_addr     Destination network address
 */
void udp_rx ( struct pk_buff *pkb, struct in_addr *src_net_addr __unused,
                        struct in_addr *dest_net_addr __unused ) {
        struct udp_header *udphdr = pkb->data;
        struct udp_connection *conn;

        /* todo: Compute and verify checksum */

        /* Print UDP header for debugging */
        DBG ( "UDP header at %#x + %d\n", udphdr, UDP_HDR_LEN  );
        DBG ( "\tSource Port = %d\n", udphdr->source_port );
        DBG ( "\tDestination Port = %d\n", udphdr->dest_port );
        DBG ( "\tLength = %d\n", udphdr->len );
        DBG ( "\tChecksum = %d\n", udphdr->chksum );
        DBG ( "\tChecksum located at %#x\n", &udphdr->chksum );

        /* Demux the connection */
        list_for_each_entry ( conn, &udp_conns, list ) {
                if ( conn->local_port == udphdr->dest_port ) {
                        goto conn;
                }
        }
        return;

        conn:
        /** Strip off the UDP header */
        pkb_pull ( pkb, UDP_HLEN );

        /** Allocate max possible buffer space to the tx buffer */
        conn->tx_pkb = alloc_pkb ( UDP_MAX_TXPKB );
        pkb_reserve ( conn->tx_pkb, UDP_MAX_HLEN );

        /** Call the application's callback */
        conn->udp_op->newdata ( conn, pkb->data, ntohs ( udphdr->len ) - UDP_HLEN  );
}

struct trans_protocol udp_protocol  = {
        .name = "UDP",
        .rx = udp_rx,
        .trans_proto = IP_UDP,
};

TRANS_PROTOCOL ( udp_protocol );

/**
 * Internal functions
 */
void copy_sockaddr ( struct sockaddr *source, struct sockaddr *dest ) {
        memcpy ( dest, source, sizeof ( struct sockaddr ) );
}
