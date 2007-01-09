#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <byteswap.h>
#include <timer.h>
#include <vsprintf.h>
#include <gpxe/pkbuff.h>
#include <gpxe/retry.h>
#include <gpxe/tcpip.h>
#include <gpxe/tcp.h>

/** @file
 *
 * TCP protocol
 *
 */

static void tcp_expired ( struct retry_timer *timer, int over );

/**
 * A TCP connection
 *
 * This data structure represents the internal state of a TCP
 * connection.  It is kept separate from @c struct @c tcp_application
 * because the internal state is still required for some time after
 * the application closes the connection.
 */
struct tcp_connection {
	/** List of TCP connections */
	struct list_head list;
	/** The associated TCP application, if any */
	struct tcp_application *app;

	/** Remote socket address */
	struct sockaddr_tcpip peer;
	/** Local port, in network byte order */
	uint16_t local_port;

	/** Current TCP state */
	unsigned int tcp_state;
	/** Previous TCP state
	 *
	 * Maintained only for debug messages
	 */
	unsigned int prev_tcp_state;
	/** Current sequence number
	 *
	 * Equivalent to SND.UNA in RFC 793 terminology.
	 */
	uint32_t snd_seq;
	/** Unacknowledged sequence count
	 *
	 * Equivalent to (SND.NXT-SND.UNA) in RFC 793 terminology.
	 */
	uint32_t snd_sent;
	/** Send window
	 *
	 * Equivalent to SND.WND in RFC 793 terminology
	 */
	uint32_t snd_win;
	/** Current acknowledgement number
	 *
	 * Equivalent to RCV.NXT in RFC 793 terminology.
	 */
	uint32_t rcv_ack;

	/** Transmit packet buffer
	 *
	 * This buffer is allocated prior to calling the application's
	 * senddata() method, to provide temporary storage space.
	 */
	struct pk_buff *tx_pkb;
	/** Retransmission timer */
	struct retry_timer timer;
};

/**
 * List of registered TCP connections
 */
static LIST_HEAD ( tcp_conns );

/**
 * Name TCP state
 *
 * @v state		TCP state
 * @ret name		Name of TCP state
 */
static inline __attribute__ (( always_inline )) const char *
tcp_state ( int state ) {
	switch ( state ) {
	case TCP_CLOSED:		return "CLOSED";
	case TCP_LISTEN:		return "LISTEN";
	case TCP_SYN_SENT:		return "SYN_SENT";
	case TCP_SYN_RCVD:		return "SYN_RCVD";
	case TCP_ESTABLISHED:		return "ESTABLISHED";
	case TCP_FIN_WAIT_1:		return "FIN_WAIT_1";
	case TCP_FIN_WAIT_2:		return "FIN_WAIT_2";
	case TCP_CLOSING_OR_LAST_ACK:	return "CLOSING/LAST_ACK";
	case TCP_TIME_WAIT:		return "TIME_WAIT";
	case TCP_CLOSE_WAIT:		return "CLOSE_WAIT";
	default:			return "INVALID";
	}
}

/**
 * Dump TCP state transition
 *
 * @v conn		TCP connection
 */
static inline __attribute__ (( always_inline )) void
tcp_dump_state ( struct tcp_connection *conn ) {

	if ( conn->tcp_state != conn->prev_tcp_state ) {
		DBGC ( conn, "TCP %p transitioned from %s to %s\n", conn,
		       tcp_state ( conn->prev_tcp_state ),
		       tcp_state ( conn->tcp_state ) );
	}
	conn->prev_tcp_state = conn->tcp_state;
}

/**
 * Dump TCP flags
 *
 * @v flags		TCP flags
 */
static inline __attribute__ (( always_inline )) void
tcp_dump_flags ( struct tcp_connection *conn, unsigned int flags ) {
	if ( flags & TCP_RST )
		DBGC ( conn, " RST" );
	if ( flags & TCP_SYN )
		DBGC ( conn, " SYN" );
	if ( flags & TCP_PSH )
		DBGC ( conn, " PSH" );
	if ( flags & TCP_FIN )
		DBGC ( conn, " FIN" );
	if ( flags & TCP_ACK )
		DBGC ( conn, " ACK" );
}

/**
 * Allocate TCP connection
 *
 * @ret conn		TCP connection, or NULL
 *
 * Allocates TCP connection and adds it to the TCP connection list.
 */
static struct tcp_connection * alloc_tcp ( void ) {
	struct tcp_connection *conn;

	conn = calloc ( 1, sizeof ( *conn ) );
	if ( conn ) {
		DBGC ( conn, "TCP %p allocated\n", conn );
		conn->tcp_state = conn->prev_tcp_state = TCP_CLOSED;
		conn->snd_seq = random();
		conn->timer.expired = tcp_expired;
		list_add ( &conn->list, &tcp_conns );
	}
	return conn;
}

/**
 * Free TCP connection
 *
 * @v conn		TCP connection
 *
 * Removes connection from TCP connection list and frees the data
 * structure.
 */
static void free_tcp ( struct tcp_connection *conn ) {

	assert ( conn );
	assert ( conn->tcp_state == TCP_CLOSED );
	assert ( conn->app == NULL );

	stop_timer ( &conn->timer );
	list_del ( &conn->list );
	free ( conn );
	DBGC ( conn, "TCP %p freed\n", conn );
}

/**
 * Associate TCP connection with application
 *
 * @v conn		TCP connection
 * @v app		TCP application
 */
static void tcp_associate ( struct tcp_connection *conn,
			    struct tcp_application *app ) {
	assert ( conn->app == NULL );
	assert ( app->conn == NULL );
	conn->app = app;
	app->conn = conn;
	DBGC ( conn, "TCP %p associated with application %p\n", conn, app );
}

/**
 * Disassociate TCP connection from application
 *
 * @v conn		TCP connection
 */
static void tcp_disassociate ( struct tcp_connection *conn ) {
	struct tcp_application *app = conn->app;

	if ( app ) {
		assert ( app->conn == conn );
		conn->app = NULL;
		app->conn = NULL;
		DBGC ( conn, "TCP %p disassociated from application %p\n",
		       conn, app );
	}
}

/**
 * Transmit any outstanding data
 *
 * @v conn		TCP connection
 * @v force_send	Force sending of packet
 * 
 * Transmits any outstanding data on the connection.  If the
 * connection is in a connected state, the application's senddata()
 * method will be called to generate the data payload, if any.
 *
 * Note that even if an error is returned, the retransmission timer
 * will have been started if necessary, and so the stack will
 * eventually attempt to retransmit the failed packet.
 */
static int tcp_senddata_conn ( struct tcp_connection *conn, int force_send ) {
	struct tcp_application *app = conn->app;
	struct pk_buff *pkb;
	struct tcp_header *tcphdr;
	unsigned int flags;
	size_t len;
	size_t seq_len;

	/* Allocate space to the TX buffer */
	pkb = alloc_pkb ( MAX_PKB_LEN );
	if ( ! pkb ) {
		DBGC ( conn, "TCP %p could not allocate data buffer\n", conn );
		/* Start the retry timer so that we attempt to
		 * retransmit this packet later.  (Start it
		 * unconditionally, since without a packet buffer we
		 * can't call the senddata() callback, and so may not
		 * be able to tell whether or not we have something
		 * that actually needs to be retransmitted).
		 */
		start_timer ( &conn->timer );
		return -ENOMEM;
	}
	pkb_reserve ( pkb, MAX_HDR_LEN );

	/* If we are connected, call the senddata() method, which may
	 * call tcp_send() to queue up a data payload.
	 */
	if ( TCP_CAN_SEND_DATA ( conn->tcp_state ) &&
	     app && app->tcp_op->senddata ) {
		conn->tx_pkb = pkb;
		app->tcp_op->senddata ( app, pkb->data, pkb_available ( pkb ));
		conn->tx_pkb = NULL;
	}

	/* Truncate payload length to fit transmit window */
	len = pkb_len ( pkb );
	if ( len > conn->snd_win )
		len = conn->snd_win;

	/* Calculate amount of sequence space that this transmission
	 * consumes.  (SYN or FIN consume one byte, and we can never
	 * send both at once).
	 */
	seq_len = len;
	flags = TCP_FLAGS_SENDING ( conn->tcp_state );
	assert ( ! ( ( flags & TCP_SYN ) && ( flags & TCP_FIN ) ) );
	if ( flags & ( TCP_SYN | TCP_FIN ) )
		seq_len++;
	conn->snd_sent = seq_len;

	/* If we have nothing to transmit, drop the packet */
	if ( ( seq_len == 0 ) && ! force_send ) {
		free_pkb ( pkb );
		return 0;
	}

	/* If we are transmitting anything that requires
	 * acknowledgement (i.e. consumes sequence space), start the
	 * retransmission timer.
	 */
	if ( seq_len )
		start_timer ( &conn->timer );

	/* Fill up the TCP header */
	tcphdr = pkb_push ( pkb, sizeof ( *tcphdr ) );
	memset ( tcphdr, 0, sizeof ( *tcphdr ) );
	tcphdr->src = conn->local_port;
	tcphdr->dest = conn->peer.st_port;
	tcphdr->seq = htonl ( conn->snd_seq );
	tcphdr->ack = htonl ( conn->rcv_ack );
	tcphdr->hlen = ( ( sizeof ( *tcphdr ) / 4 ) << 4 );
	tcphdr->flags = flags;
	tcphdr->win = htons ( TCP_WINDOW_SIZE );
	tcphdr->csum = tcpip_chksum ( pkb->data, pkb_len ( pkb ) );

	/* Dump header */
	DBGC ( conn, "TCP %p TX %d->%d %08lx..%08lx           %08lx %4zd",
	       conn, ntohs ( tcphdr->src ), ntohs ( tcphdr->dest ),
	       ntohl ( tcphdr->seq ), ( ntohl ( tcphdr->seq ) + seq_len ),
	       ntohl ( tcphdr->ack ), len );
	tcp_dump_flags ( conn, tcphdr->flags );
	DBGC ( conn, "\n" );

	/* Transmit packet */
	return tcpip_tx ( pkb, &tcp_protocol, &conn->peer, &tcphdr->csum );
}

/**
 * Transmit any outstanding data
 *
 * @v conn	TCP connection
 * 
 * This function allocates space to the transmit buffer and invokes
 * the senddata() callback function, to allow the application to
 * transmit new data.
 */
int tcp_senddata ( struct tcp_application *app ) {
	struct tcp_connection *conn = app->conn;

	/* Check connection actually exists */
	if ( ! conn ) {
		DBG ( "TCP app %p has no connection\n", app );
		return -ENOTCONN;
	}

	return tcp_senddata_conn ( conn, 0 );
}

/**
 * Transmit data
 *
 * @v app		TCP application
 * @v data		Data to be sent
 * @v len		Length of the data
 * @ret rc		Return status code
 *
 * This function queues data to be sent via the TCP connection.  It
 * can be called only in the context of an application's senddata()
 * method.
 */
int tcp_send ( struct tcp_application *app, const void *data, size_t len ) {
	struct tcp_connection *conn = app->conn;
	struct pk_buff *pkb;

	/* Check connection actually exists */
	if ( ! conn ) {
		DBG ( "TCP app %p has no connection\n", app );
		return -ENOTCONN;
	}

	/* Check that we have a packet buffer to fill */
	pkb = conn->tx_pkb;
	if ( ! pkb ) {
		DBG ( "TCP app %p tried to send data outside of the "
		      "senddata() method\n", app );
		return -EINVAL;
	}

	/* Truncate length to fit packet buffer */
	if ( len > pkb_available ( pkb ) )
		len = pkb_available ( pkb );

	/* Copy payload */
	memmove ( pkb_put ( pkb, len ), data, len );

	return 0;
}

/**
 * Retransmission timer expired
 *
 * @v timer	Retry timer
 * @v over	Failure indicator
 */
static void tcp_expired ( struct retry_timer *timer, int over ) {
	struct tcp_connection *conn =
		container_of ( timer, struct tcp_connection, timer );
	struct tcp_application *app = conn->app;
	int graceful_close = TCP_CLOSED_GRACEFULLY ( conn->tcp_state );

	DBGC ( conn, "TCP %p timer %s in %s\n", conn,
	       ( over ? "expired" : "fired" ), tcp_state ( conn->tcp_state ) );

	assert ( ( conn->tcp_state == TCP_SYN_SENT ) ||
		 ( conn->tcp_state == TCP_SYN_RCVD ) ||
		 ( conn->tcp_state == TCP_ESTABLISHED ) ||
		 ( conn->tcp_state == TCP_FIN_WAIT_1 ) ||
		 ( conn->tcp_state == TCP_TIME_WAIT ) ||
		 ( conn->tcp_state == TCP_CLOSE_WAIT ) ||
		 ( conn->tcp_state == TCP_CLOSING_OR_LAST_ACK ) );

	/* If we have finally timed out and given up, or if this is
	 * the result of a graceful close, terminate the connection
	 */
	if ( over || graceful_close ) {

		/* Transition to CLOSED */
		conn->tcp_state = TCP_CLOSED;
		tcp_dump_state ( conn );

		/* If we haven't closed gracefully, send a RST */
		if ( ! graceful_close )
			tcp_senddata_conn ( conn, 1 );

		/* Break association between application and connection */
		tcp_disassociate ( conn );

		/* Free the connection */
		free_tcp ( conn );

		/* Notify application */
		if ( app && app->tcp_op->closed )
			app->tcp_op->closed ( app, -ETIMEDOUT );

	} else {
		/* Otherwise, retransmit the packet */
		tcp_senddata_conn ( conn, 0 );
	}
}

/**
 * Send RST response to incoming packet
 *
 * @v in_tcphdr		TCP header of incoming packet
 * @ret rc		Return status code
 */
static int tcp_send_reset ( struct tcp_connection *conn,
			    struct tcp_header *in_tcphdr ) {
	struct pk_buff *pkb;
	struct tcp_header *tcphdr;

	/* Allocate space for dataless TX buffer */
	pkb = alloc_pkb ( MAX_HDR_LEN );
	if ( ! pkb ) {
		DBGC ( conn, "TCP %p could not allocate data buffer\n", conn );
		return -ENOMEM;
	}
	pkb_reserve ( pkb, MAX_HDR_LEN );

	/* Construct RST response */
	tcphdr = pkb_push ( pkb, sizeof ( *tcphdr ) );
	memset ( tcphdr, 0, sizeof ( *tcphdr ) );
	tcphdr->src = in_tcphdr->dest;
	tcphdr->dest = in_tcphdr->src;
	tcphdr->seq = in_tcphdr->ack;
	tcphdr->ack = in_tcphdr->seq;
	tcphdr->hlen = ( ( sizeof ( *tcphdr ) / 4 ) << 4 );
	tcphdr->flags = ( TCP_RST | TCP_ACK );
	tcphdr->win = htons ( TCP_WINDOW_SIZE );
	tcphdr->csum = tcpip_chksum ( pkb->data, pkb_len ( pkb ) );

	/* Dump header */
	DBGC ( conn, "TCP %p TX %d->%d %08lx..%08lx           %08lx %4zd",
	       conn, ntohs ( tcphdr->src ), ntohs ( tcphdr->dest ),
	       ntohl ( tcphdr->seq ), ( ntohl ( tcphdr->seq ) ),
	       ntohl ( tcphdr->ack ), 0 );
	tcp_dump_flags ( conn, tcphdr->flags );
	DBGC ( conn, "\n" );

	/* Transmit packet */
	return tcpip_tx ( pkb, &tcp_protocol, &conn->peer, &tcphdr->csum );
}

/**
 * Identify TCP connection by local port number
 *
 * @v local_port	Local port (in network-endian order)
 * @ret conn		TCP connection, or NULL
 */
static struct tcp_connection * tcp_demux ( uint16_t local_port ) {
	struct tcp_connection *conn;

	list_for_each_entry ( conn, &tcp_conns, list ) {
		if ( conn->local_port == local_port )
			return conn;
	}
	return NULL;
}

/**
 * Handle TCP received SYN
 *
 * @v conn		TCP connection
 * @v seq		SEQ value (in host-endian order)
 * @ret rc		Return status code
 */
static int tcp_rx_syn ( struct tcp_connection *conn, uint32_t seq ) {
	struct tcp_application *app = conn->app;

	/* Synchronise sequence numbers on first SYN */
	if ( ! ( conn->tcp_state & TCP_STATE_RCVD ( TCP_SYN ) ) )
		conn->rcv_ack = seq;

	/* Ignore duplicate SYN */
	if ( ( conn->rcv_ack - seq ) > 0 )
		return 0;

	/* Mark SYN as received and start sending ACKs with each packet */
	conn->tcp_state |= ( TCP_STATE_SENT ( TCP_ACK ) |
			     TCP_STATE_RCVD ( TCP_SYN ) );

	/* Acknowledge SYN */
	conn->rcv_ack++;

	/* Notify application of established connection, if applicable */
	if ( ( conn->tcp_state & TCP_STATE_ACKED ( TCP_SYN ) ) &&
	     app && app->tcp_op->connected )
		app->tcp_op->connected ( app );

	return 0;
}

/**
 * Handle TCP received ACK
 *
 * @v conn		TCP connection
 * @v ack		ACK value (in host-endian order)
 * @v win		WIN value (in host-endian order)
 * @ret rc		Return status code
 */
static int tcp_rx_ack ( struct tcp_connection *conn, uint32_t ack,
			uint32_t win ) {
	struct tcp_application *app = conn->app;
	size_t ack_len = ( ack - conn->snd_seq );
	size_t len;
	unsigned int acked_flags = 0;

	/* Ignore duplicate or out-of-range ACK */
	if ( ack_len > conn->snd_sent ) {
		DBGC ( conn, "TCP %p received ACK for [%08lx,%08lx), "
		       "sent only [%08lx,%08lx)\n", conn, conn->snd_seq,
		       ( conn->snd_seq + ack_len ), conn->snd_seq,
		       ( conn->snd_seq + conn->snd_sent ) );
		return -EINVAL;
	}

	/* If we are sending flags and this ACK acknowledges all
	 * outstanding sequence points, then it acknowledges the
	 * flags.  (This works since both SYN and FIN will always be
	 * the last outstanding sequence point.)
	 */
	len = ack_len;
	if ( ack_len == conn->snd_sent ) {
		acked_flags = ( TCP_FLAGS_SENDING ( conn->tcp_state ) &
				( TCP_SYN | TCP_FIN ) );
		if ( acked_flags )
			len--;
	}

	/* Update SEQ and sent counters, and window size */
	conn->snd_seq = ack;
	conn->snd_sent = 0;
	conn->snd_win = win;

	/* Stop the retransmission timer */
	stop_timer ( &conn->timer );

	/* Notify application of acknowledged data, if any */
	if ( len && app && app->tcp_op->acked )
		app->tcp_op->acked ( app, len );

	/* Mark SYN/FIN as acknowledged if applicable. */
	if ( acked_flags )
		conn->tcp_state |= TCP_STATE_ACKED ( acked_flags );

	/* Notify application of established connection, if applicable */
	if ( ( acked_flags & TCP_SYN ) &&
	     ( conn->tcp_state & TCP_STATE_RCVD ( TCP_SYN ) ) &&
	     app && app->tcp_op->connected )
		app->tcp_op->connected ( app );

	return 0;
}

/**
 * Handle TCP received data
 *
 * @v conn		TCP connection
 * @v seq		SEQ value (in host-endian order)
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
static int tcp_rx_data ( struct tcp_connection *conn, uint32_t seq,
			 void *data, size_t len ) {
	struct tcp_application *app = conn->app;
	size_t already_rcvd;

	/* Ignore duplicate data */
	already_rcvd = ( conn->rcv_ack - seq );
	if ( already_rcvd >= len )
		return 0;
	data += already_rcvd;
	len -= already_rcvd;

	/* Acknowledge new data */
	conn->rcv_ack += len;

	/* Notify application */
	if ( app && app->tcp_op->newdata )
		app->tcp_op->newdata ( app, data, len );

	return 0;
}

/**
 * Handle TCP received FIN
 *
 * @v conn		TCP connection
 * @v seq		SEQ value (in host-endian order)
 * @ret rc		Return status code
 */
static int tcp_rx_fin ( struct tcp_connection *conn, uint32_t seq ) {
	struct tcp_application *app = conn->app;

	/* Ignore duplicate FIN */
	if ( ( conn->rcv_ack - seq ) > 0 )
		return 0;

	/* Mark FIN as received, acknowledge it, and send our own FIN */
	conn->tcp_state |= ( TCP_STATE_RCVD ( TCP_FIN ) |
			     TCP_STATE_SENT ( TCP_FIN ) );
	conn->rcv_ack++;

	/* Break association with application */
	tcp_disassociate ( conn );

	/* Notify application */
	if ( app && app->tcp_op->closed )
		app->tcp_op->closed ( app, 0 );

	return 0;
}

/**
 * Handle TCP received RST
 *
 * @v conn		TCP connection
 * @v seq		SEQ value (in host-endian order)
 * @ret rc		Return status code
 */
static int tcp_rx_rst ( struct tcp_connection *conn, uint32_t seq ) {
	struct tcp_application *app = conn->app;

	/* Accept RST only if it falls within the window.  If we have
	 * not yet received a SYN, then we have no window to test
	 * against, so fall back to checking that our SYN has been
	 * ACKed.
	 */
	if ( conn->tcp_state & TCP_STATE_RCVD ( TCP_SYN ) ) {
		if ( ( conn->rcv_ack - seq ) > 0 )
			return 0;
	} else {
		if ( ! ( conn->tcp_state & TCP_STATE_ACKED ( TCP_SYN ) ) )
			return 0;
	}

	/* Transition to CLOSED */
	conn->tcp_state = TCP_CLOSED;
	tcp_dump_state ( conn );

	/* Break association between application and connection */
	tcp_disassociate ( conn );
	
	/* Free the connection */
	free_tcp ( conn );
	
	/* Notify application */
	if ( app && app->tcp_op->closed )
		app->tcp_op->closed ( app, -ECONNRESET );

	return -ECONNRESET;
}

/**
 * Process received packet
 *
 * @v pkb		Packet buffer
 * @v st_src		Partially-filled source address
 * @v st_dest		Partially-filled destination address
 * @v pshdr_csum	Pseudo-header checksum
 * @ret rc		Return status code
  */
static int tcp_rx ( struct pk_buff *pkb,
		    struct sockaddr_tcpip *st_src __unused,
		    struct sockaddr_tcpip *st_dest __unused,
		    uint16_t pshdr_csum ) {
	struct tcp_header *tcphdr = pkb->data;
	struct tcp_connection *conn;
	unsigned int hlen;
	uint16_t csum;
	uint32_t start_seq;
	uint32_t seq;
	uint32_t ack;
	uint32_t win;
	unsigned int flags;
	void *data;
	size_t len;
	int rc;

	/* Sanity check packet */
	if ( pkb_len ( pkb ) < sizeof ( *tcphdr ) ) {
		DBG ( "TCP packet too short at %d bytes (min %d bytes)\n",
		      pkb_len ( pkb ), sizeof ( *tcphdr ) );
		rc = -EINVAL;
		goto done;
	}
	hlen = ( ( tcphdr->hlen & TCP_MASK_HLEN ) / 16 ) * 4;
	if ( hlen < sizeof ( *tcphdr ) ) {
		DBG ( "TCP header too short at %d bytes (min %d bytes)\n",
		      hlen, sizeof ( *tcphdr ) );
		rc = -EINVAL;
		goto done;
	}
	if ( hlen > pkb_len ( pkb ) ) {
		DBG ( "TCP header too long at %d bytes (max %d bytes)\n",
		      hlen, pkb_len ( pkb ) );
		rc = -EINVAL;
		goto done;
	}
	csum = tcpip_continue_chksum ( pshdr_csum, pkb->data, pkb_len ( pkb ));
	if ( csum != 0 ) {
		DBG ( "TCP checksum incorrect (is %04x including checksum "
		      "field, should be 0000)\n", csum );
		rc = -EINVAL;
		goto done;
	}
	
	/* Parse parameters from header and strip header */
	conn = tcp_demux ( tcphdr->dest );
	start_seq = seq = ntohl ( tcphdr->seq );
	ack = ntohl ( tcphdr->ack );
	win = ntohs ( tcphdr->win );
	flags = tcphdr->flags;
	data = pkb_pull ( pkb, hlen );
	len = pkb_len ( pkb );

	/* Dump header */
	DBGC ( conn, "TCP %p RX %d<-%d           %08lx %08lx..%08lx %4zd",
	       conn, ntohs ( tcphdr->dest ), ntohs ( tcphdr->src ),
	       ntohl ( tcphdr->ack ), ntohl ( tcphdr->seq ),
	       ( ntohl ( tcphdr->seq ) + len +
		 ( ( tcphdr->flags & ( TCP_SYN | TCP_FIN ) ) ? 1 : 0 ) ), len);
	tcp_dump_flags ( conn, tcphdr->flags );
	DBGC ( conn, "\n" );

	/* If no connection was found, send RST */
	if ( ! conn ) {
		tcp_send_reset ( conn, tcphdr );
		rc = -ENOTCONN;
		goto done;
	}

	/* Handle ACK, if present */
	if ( flags & TCP_ACK ) {
		if ( ( rc = tcp_rx_ack ( conn, ack, win ) ) != 0 ) {
			tcp_send_reset ( conn, tcphdr );
			goto done;
		}
	}

	/* Handle SYN, if present */
	if ( flags & TCP_SYN ) {
		tcp_rx_syn ( conn, seq );
		seq++;
	}

	/* Handle RST, if present */
	if ( flags & TCP_RST ) {
		if ( ( rc = tcp_rx_rst ( conn, seq ) ) != 0 )
			goto done;
	}

	/* Handle new data, if any */
	tcp_rx_data ( conn, seq, data, len );
	seq += len;

	/* Handle FIN, if present */
	if ( flags & TCP_FIN ) {
		tcp_rx_fin ( conn, seq );
		seq++;
	}

	/* Dump out any state change as a result of the received packet */
	tcp_dump_state ( conn );

	/* Send out any pending data.  If peer is expecting an ACK for
	 * this packet then force sending a reply.
	 */
	tcp_senddata_conn ( conn, ( start_seq != seq ) );

	/* If this packet was the last we expect to receive, set up
	 * timer to expire and cause the connection to be freed.
	 */
	if ( TCP_CLOSED_GRACEFULLY ( conn->tcp_state ) ) {
		conn->timer.timeout = ( 2 * TCP_MSL );
		start_timer ( &conn->timer );
	}

	rc = 0;
 done:
	/* Free received packet */
	free_pkb ( pkb );
	return rc;
}

/**
 * Bind TCP connection to local port
 *
 * @v conn		TCP connection
 * @v local_port	Local port (in network byte order), or 0
 * @ret rc		Return status code
 *
 * This function adds the connection to the list of registered TCP
 * connections.  If the local port is 0, the connection is assigned an
 * available port between 1024 and 65535.
 */
static int tcp_bind ( struct tcp_connection *conn, uint16_t local_port ) {
	struct tcp_connection *existing;
	static uint16_t try_port = 1024;

	/* If no port specified, find the first available port */
	if ( ! local_port ) {
		for ( ; try_port ; try_port++ ) {
			if ( try_port < 1024 )
				continue;
			if ( tcp_bind ( conn, htons ( try_port ) ) == 0 )
				return 0;
		}
		DBGC ( conn, "TCP %p could not bind: no free ports\n", conn );
		return -EADDRINUSE;
	}

	/* Attempt bind to local port */
	list_for_each_entry ( existing, &tcp_conns, list ) {
		if ( existing->local_port == local_port ) {
			DBGC ( conn, "TCP %p could not bind: port %d in use\n",
			       conn, ntohs ( local_port ) );
			return -EADDRINUSE;
		}
	}
	conn->local_port = local_port;

	DBGC ( conn, "TCP %p bound to port %d\n", conn, ntohs ( local_port ) );
	return 0;
}

/**
 * Connect to a remote server
 *
 * @v app		TCP application
 * @v peer		Remote socket address
 * @v local_port	Local port number (in network byte order), or 0
 * @ret rc		Return status code
 *
 * This function initiates a TCP connection to the socket address specified in
 * peer. It sends a SYN packet to peer. When the connection is established, the
 * TCP stack calls the connected() callback function.
 */
int tcp_connect ( struct tcp_application *app, struct sockaddr_tcpip *peer,
		  uint16_t local_port ) {
	struct tcp_connection *conn;
	int rc;

	/* Application must not already have an open connection */
	if ( app->conn ) {
		DBG ( "TCP app %p already open on %p\n", app, app->conn );
		return -EISCONN;
	}

	/* Allocate connection state storage and add to connection list */
	conn = alloc_tcp();
	if ( ! conn ) {
		DBG ( "TCP app %p could not allocate connection\n", app );
		return -ENOMEM;
	}

	/* Bind to peer and to local port */
	memcpy ( &conn->peer, peer, sizeof ( conn->peer ) );
	if ( ( rc = tcp_bind ( conn, local_port ) ) != 0 ) {
		free_tcp ( conn );
		return rc;
	}

	/* Associate with application */
	tcp_associate ( conn, app );

	/* Transition to TCP_SYN_SENT and send the SYN */
	conn->tcp_state = TCP_SYN_SENT;
	tcp_dump_state ( conn );
	tcp_senddata_conn ( conn, 0 );

	return 0;
}

/**
 * Close the connection
 *
 * @v app		TCP application
 *
 * The association between the application and the TCP connection is
 * immediately severed, and the TCP application data structure can be
 * reused or freed immediately.  The TCP connection will persist until
 * the state machine has returned to the TCP_CLOSED state.
 */
void tcp_close ( struct tcp_application *app ) {
	struct tcp_connection *conn = app->conn;

	/* If no connection exists, do nothing */
	if ( ! conn )
		return;

	/* Break association between application and connection */
	tcp_disassociate ( conn );

	/* If we have not yet received a SYN (i.e. we are in CLOSED,
	 * LISTEN or SYN_SENT), just delete the connection
	 */
	if ( ! ( conn->tcp_state & TCP_STATE_RCVD ( TCP_SYN ) ) ) {
		conn->tcp_state = TCP_CLOSED;
		tcp_dump_state ( conn );
		free_tcp ( conn );
		return;
	}

	/* If we have not had our SYN acknowledged (i.e. we are in
	 * SYN_RCVD), pretend that it has been acknowledged so that we
	 * can send a FIN without breaking things.
	 */
	if ( ! ( conn->tcp_state & TCP_STATE_ACKED ( TCP_SYN ) ) )
		tcp_rx_ack ( conn, ( conn->snd_seq + 1 ), 0 );

	/* Send a FIN to initiate the close */
	conn->tcp_state |= TCP_STATE_SENT ( TCP_FIN );
	tcp_dump_state ( conn );
	tcp_senddata_conn ( conn, 0 );
}

/** TCP protocol */
struct tcpip_protocol tcp_protocol __tcpip_protocol = {
	.name = "TCP",
	.rx = tcp_rx,
	.tcpip_proto = IP_TCP,
};
