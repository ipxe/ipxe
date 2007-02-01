#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <byteswap.h>
#include <timer.h>
#include <gpxe/pkbuff.h>
#include <gpxe/malloc.h>
#include <gpxe/retry.h>
#include <gpxe/stream.h>
#include <gpxe/tcpip.h>
#include <gpxe/tcp.h>

/** @file
 *
 * TCP protocol
 *
 */

struct tcp_connection;
static void tcp_expired ( struct retry_timer *timer, int over );
static int tcp_senddata_conn ( struct tcp_connection *tcp, int force_send );
static struct stream_connection_operations tcp_op;

/**
 * A TCP connection
 *
 * This data structure represents the internal state of a TCP
 * connection.
 */
struct tcp_connection {
	/** The stream connection */
	struct stream_connection stream;
	/** List of TCP connections */
	struct list_head list;

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
 * @v tcp		TCP connection
 */
static inline __attribute__ (( always_inline )) void
tcp_dump_state ( struct tcp_connection *tcp ) {

	if ( tcp->tcp_state != tcp->prev_tcp_state ) {
		DBGC ( tcp, "TCP %p transitioned from %s to %s\n", tcp,
		       tcp_state ( tcp->prev_tcp_state ),
		       tcp_state ( tcp->tcp_state ) );
	}
	tcp->prev_tcp_state = tcp->tcp_state;
}

/**
 * Dump TCP flags
 *
 * @v flags		TCP flags
 */
static inline __attribute__ (( always_inline )) void
tcp_dump_flags ( struct tcp_connection *tcp, unsigned int flags ) {
	if ( flags & TCP_RST )
		DBGC ( tcp, " RST" );
	if ( flags & TCP_SYN )
		DBGC ( tcp, " SYN" );
	if ( flags & TCP_PSH )
		DBGC ( tcp, " PSH" );
	if ( flags & TCP_FIN )
		DBGC ( tcp, " FIN" );
	if ( flags & TCP_ACK )
		DBGC ( tcp, " ACK" );
}

/**
 * Allocate TCP connection
 *
 * @ret conn		TCP connection, or NULL
 *
 * Allocates TCP connection and adds it to the TCP connection list.
 */
static struct tcp_connection * alloc_tcp ( void ) {
	struct tcp_connection *tcp;

	tcp = malloc ( sizeof ( *tcp ) );
	if ( tcp ) {
		DBGC ( tcp, "TCP %p allocated\n", tcp );
		memset ( tcp, 0, sizeof ( *tcp ) );
		tcp->tcp_state = tcp->prev_tcp_state = TCP_CLOSED;
		tcp->snd_seq = random();
		tcp->timer.expired = tcp_expired;
		tcp->stream.op = &tcp_op;
		list_add ( &tcp->list, &tcp_conns );
	}
	return tcp;
}

/**
 * Free TCP connection
 *
 * @v tcp		TCP connection
 *
 * Removes connection from TCP connection list and frees the data
 * structure.
 */
static void free_tcp ( struct tcp_connection *tcp ) {

	assert ( tcp );
	assert ( tcp->tcp_state == TCP_CLOSED );

	stop_timer ( &tcp->timer );
	list_del ( &tcp->list );
	free ( tcp );
	DBGC ( tcp, "TCP %p freed\n", tcp );
}

/**
 * Abort TCP connection
 *
 * @v tcp		TCP connection
 * @v send_rst		Send a RST after closing
 * @v rc		Reason code
 */
static void tcp_abort ( struct tcp_connection *tcp, int send_rst, int rc ) {

	/* Transition to CLOSED */
	tcp->tcp_state = TCP_CLOSED;
	tcp_dump_state ( tcp );

	/* Send RST if requested to do so */
	if ( send_rst )
		tcp_senddata_conn ( tcp, 1 );

	/* Close stream */
	stream_closed ( &tcp->stream, rc );

	/* Free the connection */
	free_tcp ( tcp );
}

/**
 * Transmit any outstanding data
 *
 * @v tcp		TCP connection
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
static int tcp_senddata_conn ( struct tcp_connection *tcp, int force_send ) {
	struct pk_buff *pkb;
	struct tcp_header *tcphdr;
	struct tcp_mss_option *mssopt;
	void *payload;
	unsigned int flags;
	size_t len;
	size_t seq_len;
	size_t window;
	int rc;

	/* Allocate space to the TX buffer */
	pkb = alloc_pkb ( MAX_PKB_LEN );
	if ( ! pkb ) {
		DBGC ( tcp, "TCP %p could not allocate data buffer\n", tcp );
		/* Start the retry timer so that we attempt to
		 * retransmit this packet later.  (Start it
		 * unconditionally, since without a packet buffer we
		 * can't call the senddata() callback, and so may not
		 * be able to tell whether or not we have something
		 * that actually needs to be retransmitted).
		 */
		start_timer ( &tcp->timer );
		return -ENOMEM;
	}
	pkb_reserve ( pkb, MAX_HDR_LEN );

	/* If we are connected, call the senddata() method, which may
	 * call tcp_send() to queue up a data payload.
	 */
	if ( TCP_CAN_SEND_DATA ( tcp->tcp_state ) ) {
		tcp->tx_pkb = pkb;
		stream_senddata ( &tcp->stream, pkb->data,
				  pkb_tailroom ( pkb ) );
		tcp->tx_pkb = NULL;
	}

	/* Truncate payload length to fit transmit window */
	len = pkb_len ( pkb );
	if ( len > tcp->snd_win )
		len = tcp->snd_win;

	/* Calculate amount of sequence space that this transmission
	 * consumes.  (SYN or FIN consume one byte, and we can never
	 * send both at once).
	 */
	seq_len = len;
	flags = TCP_FLAGS_SENDING ( tcp->tcp_state );
	assert ( ! ( ( flags & TCP_SYN ) && ( flags & TCP_FIN ) ) );
	if ( flags & ( TCP_SYN | TCP_FIN ) )
		seq_len++;
	tcp->snd_sent = seq_len;

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
		start_timer ( &tcp->timer );

	/* Estimate window size */
	window = ( ( freemem * 3 ) / 4 );
	if ( window > TCP_MAX_WINDOW_SIZE )
		window = TCP_MAX_WINDOW_SIZE;
	window &= ~0x03; /* Keep everything dword-aligned */

	/* Fill up the TCP header */
	payload = pkb->data;
	if ( flags & TCP_SYN ) {
		mssopt = pkb_push ( pkb, sizeof ( *mssopt ) );
		mssopt->kind = TCP_OPTION_MSS;
		mssopt->length = sizeof ( *mssopt );
		mssopt->mss = htons ( TCP_MSS );
	}
	tcphdr = pkb_push ( pkb, sizeof ( *tcphdr ) );
	memset ( tcphdr, 0, sizeof ( *tcphdr ) );
	tcphdr->src = tcp->local_port;
	tcphdr->dest = tcp->peer.st_port;
	tcphdr->seq = htonl ( tcp->snd_seq );
	tcphdr->ack = htonl ( tcp->rcv_ack );
	tcphdr->hlen = ( ( payload - pkb->data ) << 2 );
	tcphdr->flags = flags;
	tcphdr->win = htons ( window );
	tcphdr->csum = tcpip_chksum ( pkb->data, pkb_len ( pkb ) );

	/* Dump header */
	DBGC ( tcp, "TCP %p TX %d->%d %08lx..%08lx           %08lx %4zd",
	       tcp, ntohs ( tcphdr->src ), ntohs ( tcphdr->dest ),
	       ntohl ( tcphdr->seq ), ( ntohl ( tcphdr->seq ) + seq_len ),
	       ntohl ( tcphdr->ack ), len );
	tcp_dump_flags ( tcp, tcphdr->flags );
	DBGC ( tcp, "\n" );

	/* Transmit packet */
	rc = tcpip_tx ( pkb, &tcp_protocol, &tcp->peer, NULL, &tcphdr->csum );

	/* If we got -ENETUNREACH, kill the connection immediately
	 * because there is no point retrying.  This isn't strictly
	 * necessary (since we will eventually time out anyway), but
	 * it avoids irritating needless delays.  Don't do this for
	 * RST packets transmitted on connection abort, to avoid a
	 * potential infinite loop.
	 */
	if ( ( ! ( tcp->tcp_state & TCP_STATE_SENT ( TCP_RST ) ) ) &&
	     ( rc == -ENETUNREACH ) ) {
		DBGC ( tcp, "TCP %p aborting after TX failed: %s\n",
		       tcp, strerror ( rc ) );
		tcp_abort ( tcp, 0, rc );
	}

	return rc;
}

/**
 * Transmit any outstanding data
 *
 * @v stream		TCP stream
 * 
 * This function allocates space to the transmit buffer and invokes
 * the senddata() callback function, to allow the application to
 * transmit new data.
 */
static int tcp_kick ( struct stream_connection *stream ) {
	struct tcp_connection *tcp =
		container_of ( stream, struct tcp_connection, stream );

	return tcp_senddata_conn ( tcp, 0 );
}

/**
 * Transmit data
 *
 * @v stream		TCP stream
 * @v data		Data to be sent
 * @v len		Length of the data
 * @ret rc		Return status code
 *
 * This function queues data to be sent via the TCP connection.  It
 * can be called only in the context of an application's senddata()
 * method.
 */
static int tcp_send ( struct stream_connection *stream,
		      const void *data, size_t len ) {
	struct tcp_connection *tcp =
		container_of ( stream, struct tcp_connection, stream );
	struct pk_buff *pkb;

	/* Check that we have a packet buffer to fill */
	pkb = tcp->tx_pkb;
	if ( ! pkb ) {
		DBGC ( tcp, "TCP %p tried to send data outside of the "
		       "senddata() method\n", tcp );
		return -EINVAL;
	}

	/* Truncate length to fit packet buffer */
	if ( len > pkb_tailroom ( pkb ) )
		len = pkb_tailroom ( pkb );

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
	struct tcp_connection *tcp =
		container_of ( timer, struct tcp_connection, timer );
	int graceful_close = TCP_CLOSED_GRACEFULLY ( tcp->tcp_state );

	DBGC ( tcp, "TCP %p timer %s in %s\n", tcp,
	       ( over ? "expired" : "fired" ), tcp_state ( tcp->tcp_state ) );

	assert ( ( tcp->tcp_state == TCP_SYN_SENT ) ||
		 ( tcp->tcp_state == TCP_SYN_RCVD ) ||
		 ( tcp->tcp_state == TCP_ESTABLISHED ) ||
		 ( tcp->tcp_state == TCP_FIN_WAIT_1 ) ||
		 ( tcp->tcp_state == TCP_TIME_WAIT ) ||
		 ( tcp->tcp_state == TCP_CLOSE_WAIT ) ||
		 ( tcp->tcp_state == TCP_CLOSING_OR_LAST_ACK ) );

	if ( over || graceful_close ) {
		/* If we have finally timed out and given up, or if
		 * this is the result of a graceful close, terminate
		 * the connection
		 */
		tcp_abort ( tcp, 1, -ETIMEDOUT );
	} else {
		/* Otherwise, retransmit the packet */
		tcp_senddata_conn ( tcp, 0 );
	}
}

/**
 * Send RST response to incoming packet
 *
 * @v in_tcphdr		TCP header of incoming packet
 * @ret rc		Return status code
 */
static int tcp_send_reset ( struct tcp_connection *tcp,
			    struct tcp_header *in_tcphdr ) {
	struct pk_buff *pkb;
	struct tcp_header *tcphdr;

	/* Allocate space for dataless TX buffer */
	pkb = alloc_pkb ( MAX_HDR_LEN );
	if ( ! pkb ) {
		DBGC ( tcp, "TCP %p could not allocate data buffer\n", tcp );
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
	tcphdr->win = htons ( TCP_MAX_WINDOW_SIZE );
	tcphdr->csum = tcpip_chksum ( pkb->data, pkb_len ( pkb ) );

	/* Dump header */
	DBGC ( tcp, "TCP %p TX %d->%d %08lx..%08lx           %08lx %4zd",
	       tcp, ntohs ( tcphdr->src ), ntohs ( tcphdr->dest ),
	       ntohl ( tcphdr->seq ), ( ntohl ( tcphdr->seq ) ),
	       ntohl ( tcphdr->ack ), 0 );
	tcp_dump_flags ( tcp, tcphdr->flags );
	DBGC ( tcp, "\n" );

	/* Transmit packet */
	return tcpip_tx ( pkb, &tcp_protocol, &tcp->peer,
			  NULL, &tcphdr->csum );
}

/**
 * Identify TCP connection by local port number
 *
 * @v local_port	Local port (in network-endian order)
 * @ret tcp		TCP connection, or NULL
 */
static struct tcp_connection * tcp_demux ( uint16_t local_port ) {
	struct tcp_connection *tcp;

	list_for_each_entry ( tcp, &tcp_conns, list ) {
		if ( tcp->local_port == local_port )
			return tcp;
	}
	return NULL;
}

/**
 * Handle TCP received SYN
 *
 * @v tcp		TCP connection
 * @v seq		SEQ value (in host-endian order)
 * @ret rc		Return status code
 */
static int tcp_rx_syn ( struct tcp_connection *tcp, uint32_t seq ) {

	/* Synchronise sequence numbers on first SYN */
	if ( ! ( tcp->tcp_state & TCP_STATE_RCVD ( TCP_SYN ) ) )
		tcp->rcv_ack = seq;

	/* Ignore duplicate SYN */
	if ( ( tcp->rcv_ack - seq ) > 0 )
		return 0;

	/* Mark SYN as received and start sending ACKs with each packet */
	tcp->tcp_state |= ( TCP_STATE_SENT ( TCP_ACK ) |
			     TCP_STATE_RCVD ( TCP_SYN ) );

	/* Acknowledge SYN */
	tcp->rcv_ack++;

	/* Notify application of established connection, if applicable */
	if ( ( tcp->tcp_state & TCP_STATE_ACKED ( TCP_SYN ) ) )
		stream_connected ( &tcp->stream );

	return 0;
}

/**
 * Handle TCP received ACK
 *
 * @v tcp		TCP connection
 * @v ack		ACK value (in host-endian order)
 * @v win		WIN value (in host-endian order)
 * @ret rc		Return status code
 */
static int tcp_rx_ack ( struct tcp_connection *tcp, uint32_t ack,
			uint32_t win ) {
	size_t ack_len = ( ack - tcp->snd_seq );
	size_t len;
	unsigned int acked_flags = 0;

	/* Ignore duplicate or out-of-range ACK */
	if ( ack_len > tcp->snd_sent ) {
		DBGC ( tcp, "TCP %p received ACK for [%08lx,%08lx), "
		       "sent only [%08lx,%08lx)\n", tcp, tcp->snd_seq,
		       ( tcp->snd_seq + ack_len ), tcp->snd_seq,
		       ( tcp->snd_seq + tcp->snd_sent ) );
		return -EINVAL;
	}

	/* If we are sending flags and this ACK acknowledges all
	 * outstanding sequence points, then it acknowledges the
	 * flags.  (This works since both SYN and FIN will always be
	 * the last outstanding sequence point.)
	 */
	len = ack_len;
	if ( ack_len == tcp->snd_sent ) {
		acked_flags = ( TCP_FLAGS_SENDING ( tcp->tcp_state ) &
				( TCP_SYN | TCP_FIN ) );
		if ( acked_flags )
			len--;
	}

	/* Update SEQ and sent counters, and window size */
	tcp->snd_seq = ack;
	tcp->snd_sent = 0;
	tcp->snd_win = win;

	/* Stop the retransmission timer */
	stop_timer ( &tcp->timer );

	/* Notify application of acknowledged data, if any */
	if ( len )
		stream_acked ( &tcp->stream, len );

	/* Mark SYN/FIN as acknowledged if applicable. */
	if ( acked_flags )
		tcp->tcp_state |= TCP_STATE_ACKED ( acked_flags );

	/* Notify application of established connection, if applicable */
	if ( ( acked_flags & TCP_SYN ) &&
	     ( tcp->tcp_state & TCP_STATE_RCVD ( TCP_SYN ) ) ) {
		stream_connected ( &tcp->stream );
	}

	return 0;
}

/**
 * Handle TCP received data
 *
 * @v tcp		TCP connection
 * @v seq		SEQ value (in host-endian order)
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
static int tcp_rx_data ( struct tcp_connection *tcp, uint32_t seq,
			 void *data, size_t len ) {
	size_t already_rcvd;

	/* Ignore duplicate data */
	already_rcvd = ( tcp->rcv_ack - seq );
	if ( already_rcvd >= len )
		return 0;
	data += already_rcvd;
	len -= already_rcvd;

	/* Acknowledge new data */
	tcp->rcv_ack += len;

	/* Notify application */
	stream_newdata ( &tcp->stream, data, len );

	return 0;
}

/**
 * Handle TCP received FIN
 *
 * @v tcp		TCP connection
 * @v seq		SEQ value (in host-endian order)
 * @ret rc		Return status code
 */
static int tcp_rx_fin ( struct tcp_connection *tcp, uint32_t seq ) {

	/* Ignore duplicate FIN */
	if ( ( tcp->rcv_ack - seq ) > 0 )
		return 0;

	/* Mark FIN as received, acknowledge it, and send our own FIN */
	tcp->tcp_state |= ( TCP_STATE_RCVD ( TCP_FIN ) |
			     TCP_STATE_SENT ( TCP_FIN ) );
	tcp->rcv_ack++;

	/* Close stream */
	stream_closed ( &tcp->stream, 0 );

	return 0;
}

/**
 * Handle TCP received RST
 *
 * @v tcp		TCP connection
 * @v seq		SEQ value (in host-endian order)
 * @ret rc		Return status code
 */
static int tcp_rx_rst ( struct tcp_connection *tcp, uint32_t seq ) {

	/* Accept RST only if it falls within the window.  If we have
	 * not yet received a SYN, then we have no window to test
	 * against, so fall back to checking that our SYN has been
	 * ACKed.
	 */
	if ( tcp->tcp_state & TCP_STATE_RCVD ( TCP_SYN ) ) {
		if ( ( tcp->rcv_ack - seq ) > 0 )
			return 0;
	} else {
		if ( ! ( tcp->tcp_state & TCP_STATE_ACKED ( TCP_SYN ) ) )
			return 0;
	}

	/* Abort connection without sending a RST */
	tcp_abort ( tcp, 0, -ECONNRESET );

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
	struct tcp_connection *tcp;
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
	tcp = tcp_demux ( tcphdr->dest );
	start_seq = seq = ntohl ( tcphdr->seq );
	ack = ntohl ( tcphdr->ack );
	win = ntohs ( tcphdr->win );
	flags = tcphdr->flags;
	data = pkb_pull ( pkb, hlen );
	len = pkb_len ( pkb );

	/* Dump header */
	DBGC ( tcp, "TCP %p RX %d<-%d           %08lx %08lx..%08lx %4zd",
	       tcp, ntohs ( tcphdr->dest ), ntohs ( tcphdr->src ),
	       ntohl ( tcphdr->ack ), ntohl ( tcphdr->seq ),
	       ( ntohl ( tcphdr->seq ) + len +
		 ( ( tcphdr->flags & ( TCP_SYN | TCP_FIN ) ) ? 1 : 0 ) ), len);
	tcp_dump_flags ( tcp, tcphdr->flags );
	DBGC ( tcp, "\n" );

	/* If no connection was found, send RST */
	if ( ! tcp ) {
		tcp_send_reset ( tcp, tcphdr );
		rc = -ENOTCONN;
		goto done;
	}

	/* Handle ACK, if present */
	if ( flags & TCP_ACK ) {
		if ( ( rc = tcp_rx_ack ( tcp, ack, win ) ) != 0 ) {
			tcp_send_reset ( tcp, tcphdr );
			goto done;
		}
	}

	/* Handle SYN, if present */
	if ( flags & TCP_SYN ) {
		tcp_rx_syn ( tcp, seq );
		seq++;
	}

	/* Handle RST, if present */
	if ( flags & TCP_RST ) {
		if ( ( rc = tcp_rx_rst ( tcp, seq ) ) != 0 )
			goto done;
	}

	/* Handle new data, if any */
	tcp_rx_data ( tcp, seq, data, len );
	seq += len;

	/* Handle FIN, if present */
	if ( flags & TCP_FIN ) {
		tcp_rx_fin ( tcp, seq );
		seq++;
	}

	/* Dump out any state change as a result of the received packet */
	tcp_dump_state ( tcp );

	/* Send out any pending data.  If peer is expecting an ACK for
	 * this packet then force sending a reply.
	 */
	tcp_senddata_conn ( tcp, ( start_seq != seq ) );

	/* If this packet was the last we expect to receive, set up
	 * timer to expire and cause the connection to be freed.
	 */
	if ( TCP_CLOSED_GRACEFULLY ( tcp->tcp_state ) ) {
		tcp->timer.timeout = ( 2 * TCP_MSL );
		start_timer ( &tcp->timer );
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
 * @v stream		TCP stream
 * @v local		Local address
 * @ret rc		Return status code
 *
 * Only the port portion of the local address is used.  If the local
 * port is 0, the connection is assigned an available port between
 * 1024 and 65535.
 */
static int tcp_bind ( struct stream_connection *stream,
		      struct sockaddr *local ) {
	struct tcp_connection *tcp =
		container_of ( stream, struct tcp_connection, stream );
	struct sockaddr_tcpip *st = ( ( struct sockaddr_tcpip * ) local );
	struct tcp_connection *existing;
	struct sockaddr_tcpip try;
	static uint16_t try_port = 1024;
	uint16_t local_port = st->st_port;

	/* If no port specified, find the first available port */
	if ( ! local_port ) {
		for ( ; try_port ; try_port++ ) {
			if ( try_port < 1024 )
				continue;
			try.st_port = htons ( try_port );
			if ( tcp_bind ( stream,
					( struct sockaddr * ) &try ) == 0 ) {
				return 0;
			}
		}
		DBGC ( tcp, "TCP %p could not bind: no free ports\n", tcp );
		return -EADDRINUSE;
	}

	/* Attempt bind to local port */
	list_for_each_entry ( existing, &tcp_conns, list ) {
		if ( existing->local_port == local_port ) {
			DBGC ( tcp, "TCP %p could not bind: port %d in use\n",
			       tcp, ntohs ( local_port ) );
			return -EADDRINUSE;
		}
	}
	tcp->local_port = local_port;

	DBGC ( tcp, "TCP %p bound to port %d\n", tcp, ntohs ( local_port ) );
	return 0;
}

/**
 * Connect to a remote server
 *
 * @v stream		TCP stream
 * @v peer		Remote socket address
 * @ret rc		Return status code
 *
 * This function initiates a TCP connection to the socket address specified in
 * peer. It sends a SYN packet to peer. When the connection is established, the
 * TCP stack calls the connected() callback function.
 */
static int tcp_connect ( struct stream_connection *stream,
			 struct sockaddr *peer ) {
	struct tcp_connection *tcp =
		container_of ( stream, struct tcp_connection, stream );
	struct sockaddr_tcpip *st = ( ( struct sockaddr_tcpip * ) peer );
	struct sockaddr_tcpip local;
	int rc;

	/* Bind to local port if not already bound */
	if ( ! tcp->local_port ) {
		local.st_port = 0;
		if ( ( rc = tcp_bind ( stream,
				       ( struct sockaddr * ) &local ) ) != 0 ){
			return rc;
		}
	}

	/* Bind to peer */
	memcpy ( &tcp->peer, st, sizeof ( tcp->peer ) );

	/* Transition to TCP_SYN_SENT and send the SYN */
	tcp->tcp_state = TCP_SYN_SENT;
	tcp_dump_state ( tcp );
	tcp_senddata_conn ( tcp, 0 );

	return 0;
}

/**
 * Close the connection
 *
 * @v stream		TCP stream
 *
 * The TCP connection will persist until the state machine has
 * returned to the TCP_CLOSED state.
 */
static void tcp_close ( struct stream_connection *stream ) {
	struct tcp_connection *tcp =
		container_of ( stream, struct tcp_connection, stream );

	/* If we have not yet received a SYN (i.e. we are in CLOSED,
	 * LISTEN or SYN_SENT), just delete the connection
	 */
	if ( ! ( tcp->tcp_state & TCP_STATE_RCVD ( TCP_SYN ) ) ) {
		tcp->tcp_state = TCP_CLOSED;
		tcp_dump_state ( tcp );
		free_tcp ( tcp );
		return;
	}

	/* If we have not had our SYN acknowledged (i.e. we are in
	 * SYN_RCVD), pretend that it has been acknowledged so that we
	 * can send a FIN without breaking things.
	 */
	if ( ! ( tcp->tcp_state & TCP_STATE_ACKED ( TCP_SYN ) ) )
		tcp_rx_ack ( tcp, ( tcp->snd_seq + 1 ), 0 );

	/* Send a FIN to initiate the close */
	tcp->tcp_state |= TCP_STATE_SENT ( TCP_FIN );
	tcp_dump_state ( tcp );
	tcp_senddata_conn ( tcp, 0 );
}

/**
 * Open TCP connection
 *
 * @v app		Stream application
 * @ret rc		Return status code
 */
int tcp_open ( struct stream_application *app ) {
	struct tcp_connection *tcp;

	/* Application must not already have an open connection */
	if ( app->conn ) {
		DBG ( "TCP app %p already open on %p\n", app, app->conn );
		return -EISCONN;
	}

	/* Allocate connection state storage and add to connection list */
	tcp = alloc_tcp();
	if ( ! tcp ) {
		DBG ( "TCP app %p could not allocate connection\n", app );
		return -ENOMEM;
	}

	/* Associate application with connection */
	stream_associate ( app, &tcp->stream );

	return 0;
}

/** TCP stream operations */
static struct stream_connection_operations tcp_op = {
	.bind		= tcp_bind,
	.connect	= tcp_connect,
	.close		= tcp_close,
	.send		= tcp_send,
	.kick		= tcp_kick,
};

/** TCP protocol */
struct tcpip_protocol tcp_protocol __tcpip_protocol = {
	.name = "TCP",
	.rx = tcp_rx,
	.tcpip_proto = IP_TCP,
};
