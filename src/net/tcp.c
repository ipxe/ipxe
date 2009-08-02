#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <byteswap.h>
#include <gpxe/timer.h>
#include <gpxe/iobuf.h>
#include <gpxe/malloc.h>
#include <gpxe/retry.h>
#include <gpxe/refcnt.h>
#include <gpxe/xfer.h>
#include <gpxe/open.h>
#include <gpxe/uri.h>
#include <gpxe/tcpip.h>
#include <gpxe/tcp.h>

/** @file
 *
 * TCP protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** A TCP connection */
struct tcp_connection {
	/** Reference counter */
	struct refcnt refcnt;
	/** List of TCP connections */
	struct list_head list;

	/** Data transfer interface */
	struct xfer_interface xfer;
	/** Data transfer interface closed flag */
	int xfer_closed;

	/** Remote socket address */
	struct sockaddr_tcpip peer;
	/** Local port, in network byte order */
	unsigned int local_port;

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
	/** Receive window
	 *
	 * Equivalent to RCV.WND in RFC 793 terminology.
	 */
	uint32_t rcv_win;
	/** Most recent received timestamp
	 *
	 * Equivalent to TS.Recent in RFC 1323 terminology.
	 */
	uint32_t ts_recent;
	/** Timestamps enabled */
	int timestamps;

	/** Transmit queue */
	struct list_head queue;
	/** Retransmission timer */
	struct retry_timer timer;
};

/**
 * List of registered TCP connections
 */
static LIST_HEAD ( tcp_conns );

/* Forward declarations */
static struct xfer_interface_operations tcp_xfer_operations;
static void tcp_expired ( struct retry_timer *timer, int over );
static int tcp_rx_ack ( struct tcp_connection *tcp, uint32_t ack,
			uint32_t win );

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
		DBGC2 ( tcp, " RST" );
	if ( flags & TCP_SYN )
		DBGC2 ( tcp, " SYN" );
	if ( flags & TCP_PSH )
		DBGC2 ( tcp, " PSH" );
	if ( flags & TCP_FIN )
		DBGC2 ( tcp, " FIN" );
	if ( flags & TCP_ACK )
		DBGC2 ( tcp, " ACK" );
}

/***************************************************************************
 *
 * Open and close
 *
 ***************************************************************************
 */

/**
 * Bind TCP connection to local port
 *
 * @v tcp		TCP connection
 * @v port		Local port number, in network-endian order
 * @ret rc		Return status code
 *
 * If the port is 0, the connection is assigned an available port
 * between 1024 and 65535.
 */
static int tcp_bind ( struct tcp_connection *tcp, unsigned int port ) {
	struct tcp_connection *existing;
	static uint16_t try_port = 1023;

	/* If no port specified, find the first available port */
	if ( ! port ) {
		while ( try_port ) {
			try_port++;
			if ( try_port < 1024 )
				continue;
			if ( tcp_bind ( tcp, htons ( try_port ) ) == 0 )
				return 0;
		}
		DBGC ( tcp, "TCP %p could not bind: no free ports\n", tcp );
		return -EADDRINUSE;
	}

	/* Attempt bind to local port */
	list_for_each_entry ( existing, &tcp_conns, list ) {
		if ( existing->local_port == port ) {
			DBGC ( tcp, "TCP %p could not bind: port %d in use\n",
			       tcp, ntohs ( port ) );
			return -EADDRINUSE;
		}
	}
	tcp->local_port = port;

	DBGC ( tcp, "TCP %p bound to port %d\n", tcp, ntohs ( port ) );
	return 0;
}

/**
 * Open a TCP connection
 *
 * @v xfer		Data transfer interface
 * @v peer		Peer socket address
 * @v local		Local socket address, or NULL
 * @ret rc		Return status code
 */
static int tcp_open ( struct xfer_interface *xfer, struct sockaddr *peer,
		      struct sockaddr *local ) {
	struct sockaddr_tcpip *st_peer = ( struct sockaddr_tcpip * ) peer;
	struct sockaddr_tcpip *st_local = ( struct sockaddr_tcpip * ) local;
	struct tcp_connection *tcp;
	unsigned int bind_port;
	int rc;

	/* Allocate and initialise structure */
	tcp = zalloc ( sizeof ( *tcp ) );
	if ( ! tcp )
		return -ENOMEM;
	DBGC ( tcp, "TCP %p allocated\n", tcp );
	xfer_init ( &tcp->xfer, &tcp_xfer_operations, &tcp->refcnt );
	tcp->prev_tcp_state = TCP_CLOSED;
	tcp->tcp_state = TCP_STATE_SENT ( TCP_SYN );
	tcp_dump_state ( tcp );
	tcp->snd_seq = random();
	INIT_LIST_HEAD ( &tcp->queue );
	tcp->timer.expired = tcp_expired;
	memcpy ( &tcp->peer, st_peer, sizeof ( tcp->peer ) );

	/* Bind to local port */
	bind_port = ( st_local ? st_local->st_port : 0 );
	if ( ( rc = tcp_bind ( tcp, bind_port ) ) != 0 )
		goto err;

	/* Start timer to initiate SYN */
	start_timer_nodelay ( &tcp->timer );

	/* Attach parent interface, transfer reference to connection
	 * list and return
	 */
	xfer_plug_plug ( &tcp->xfer, xfer );
	list_add ( &tcp->list, &tcp_conns );
	return 0;

 err:
	ref_put ( &tcp->refcnt );
	return rc;
}

/**
 * Close TCP connection
 *
 * @v tcp		TCP connection
 * @v rc		Reason for close
 *
 * Closes the data transfer interface.  If the TCP state machine is in
 * a suitable state, the connection will be deleted.
 */
static void tcp_close ( struct tcp_connection *tcp, int rc ) {
	struct io_buffer *iobuf;
	struct io_buffer *tmp;

	/* Close data transfer interface */
	xfer_nullify ( &tcp->xfer );
	xfer_close ( &tcp->xfer, rc );
	tcp->xfer_closed = 1;

	/* If we are in CLOSED, or have otherwise not yet received a
	 * SYN (i.e. we are in LISTEN or SYN_SENT), just delete the
	 * connection.
	 */
	if ( ! ( tcp->tcp_state & TCP_STATE_RCVD ( TCP_SYN ) ) ) {

		/* Transition to CLOSED for the sake of debugging messages */
		tcp->tcp_state = TCP_CLOSED;
		tcp_dump_state ( tcp );

		/* Free any unsent I/O buffers */
		list_for_each_entry_safe ( iobuf, tmp, &tcp->queue, list ) {
			list_del ( &iobuf->list );
			free_iob ( iobuf );
		}

		/* Remove from list and drop reference */
		stop_timer ( &tcp->timer );
		list_del ( &tcp->list );
		ref_put ( &tcp->refcnt );
		DBGC ( tcp, "TCP %p connection deleted\n", tcp );
		return;
	}

	/* If we have not had our SYN acknowledged (i.e. we are in
	 * SYN_RCVD), pretend that it has been acknowledged so that we
	 * can send a FIN without breaking things.
	 */
	if ( ! ( tcp->tcp_state & TCP_STATE_ACKED ( TCP_SYN ) ) )
		tcp_rx_ack ( tcp, ( tcp->snd_seq + 1 ), 0 );

	/* If we have no data remaining to send, start sending FIN */
	if ( list_empty ( &tcp->queue ) ) {
		tcp->tcp_state |= TCP_STATE_SENT ( TCP_FIN );
		tcp_dump_state ( tcp );
	}
}

/***************************************************************************
 *
 * Transmit data path
 *
 ***************************************************************************
 */

/**
 * Calculate transmission window
 *
 * @v tcp		TCP connection
 * @ret len		Maximum length that can be sent in a single packet
 */
static size_t tcp_xmit_win ( struct tcp_connection *tcp ) {
	size_t len;

	/* Not ready if we're not in a suitable connection state */
	if ( ! TCP_CAN_SEND_DATA ( tcp->tcp_state ) )
		return 0;

	/* Length is the minimum of the receiver's window and the path MTU */
	len = tcp->snd_win;
	if ( len > TCP_PATH_MTU )
		len = TCP_PATH_MTU;

	return len;
}

/**
 * Process TCP transmit queue
 *
 * @v tcp		TCP connection
 * @v max_len		Maximum length to process
 * @v dest		I/O buffer to fill with data, or NULL
 * @v remove		Remove data from queue
 * @ret len		Length of data processed
 *
 * This processes at most @c max_len bytes from the TCP connection's
 * transmit queue.  Data will be copied into the @c dest I/O buffer
 * (if provided) and, if @c remove is true, removed from the transmit
 * queue.
 */
static size_t tcp_process_queue ( struct tcp_connection *tcp, size_t max_len,
				  struct io_buffer *dest, int remove ) {
	struct io_buffer *iobuf;
	struct io_buffer *tmp;
	size_t frag_len;
	size_t len = 0;

	list_for_each_entry_safe ( iobuf, tmp, &tcp->queue, list ) {
		frag_len = iob_len ( iobuf );
		if ( frag_len > max_len )
			frag_len = max_len;
		if ( dest ) {
			memcpy ( iob_put ( dest, frag_len ), iobuf->data,
				 frag_len );
		}
		if ( remove ) {
			iob_pull ( iobuf, frag_len );
			if ( ! iob_len ( iobuf ) ) {
				list_del ( &iobuf->list );
				free_iob ( iobuf );
			}
		}
		len += frag_len;
		max_len -= frag_len;
	}
	return len;
}

/**
 * Transmit any outstanding data
 *
 * @v tcp		TCP connection
 * @v force_send	Force sending of packet
 * 
 * Transmits any outstanding data on the connection.
 *
 * Note that even if an error is returned, the retransmission timer
 * will have been started if necessary, and so the stack will
 * eventually attempt to retransmit the failed packet.
 */
static int tcp_xmit ( struct tcp_connection *tcp, int force_send ) {
	struct io_buffer *iobuf;
	struct tcp_header *tcphdr;
	struct tcp_mss_option *mssopt;
	struct tcp_timestamp_padded_option *tsopt;
	void *payload;
	unsigned int flags;
	size_t len = 0;
	uint32_t seq_len;
	uint32_t app_win;
	uint32_t max_rcv_win;
	int rc;

	/* If retransmission timer is already running, do nothing */
	if ( timer_running ( &tcp->timer ) )
		return 0;

	/* Calculate both the actual (payload) and sequence space
	 * lengths that we wish to transmit.
	 */
	if ( TCP_CAN_SEND_DATA ( tcp->tcp_state ) ) {
		len = tcp_process_queue ( tcp, tcp_xmit_win ( tcp ),
					  NULL, 0 );
	}
	seq_len = len;
	flags = TCP_FLAGS_SENDING ( tcp->tcp_state );
	if ( flags & ( TCP_SYN | TCP_FIN ) ) {
		/* SYN or FIN consume one byte, and we can never send both */
		assert ( ! ( ( flags & TCP_SYN ) && ( flags & TCP_FIN ) ) );
		seq_len++;
	}
	tcp->snd_sent = seq_len;

	/* If we have nothing to transmit, stop now */
	if ( ( seq_len == 0 ) && ! force_send )
		return 0;

	/* If we are transmitting anything that requires
	 * acknowledgement (i.e. consumes sequence space), start the
	 * retransmission timer.  Do this before attempting to
	 * allocate the I/O buffer, in case allocation itself fails.
	 */
	if ( seq_len )
		start_timer ( &tcp->timer );

	/* Allocate I/O buffer */
	iobuf = alloc_iob ( len + MAX_HDR_LEN );
	if ( ! iobuf ) {
		DBGC ( tcp, "TCP %p could not allocate iobuf for %08x..%08x "
		       "%08x\n", tcp, tcp->snd_seq, ( tcp->snd_seq + seq_len ),
		       tcp->rcv_ack );
		return -ENOMEM;
	}
	iob_reserve ( iobuf, MAX_HDR_LEN );

	/* Fill data payload from transmit queue */
	tcp_process_queue ( tcp, len, iobuf, 0 );

	/* Expand receive window if possible */
	max_rcv_win = ( ( freemem * 3 ) / 4 );
	if ( max_rcv_win > TCP_MAX_WINDOW_SIZE )
		max_rcv_win = TCP_MAX_WINDOW_SIZE;
	app_win = xfer_window ( &tcp->xfer );
	if ( max_rcv_win > app_win )
		max_rcv_win = app_win;
	max_rcv_win &= ~0x03; /* Keep everything dword-aligned */
	if ( tcp->rcv_win < max_rcv_win )
		tcp->rcv_win = max_rcv_win;

	/* Fill up the TCP header */
	payload = iobuf->data;
	if ( flags & TCP_SYN ) {
		mssopt = iob_push ( iobuf, sizeof ( *mssopt ) );
		mssopt->kind = TCP_OPTION_MSS;
		mssopt->length = sizeof ( *mssopt );
		mssopt->mss = htons ( TCP_MSS );
	}
	if ( ( flags & TCP_SYN ) || tcp->timestamps ) {
		tsopt = iob_push ( iobuf, sizeof ( *tsopt ) );
		memset ( tsopt->nop, TCP_OPTION_NOP, sizeof ( tsopt->nop ) );
		tsopt->tsopt.kind = TCP_OPTION_TS;
		tsopt->tsopt.length = sizeof ( tsopt->tsopt );
		tsopt->tsopt.tsval = ntohl ( currticks() );
		tsopt->tsopt.tsecr = ntohl ( tcp->ts_recent );
	}
	if ( ! ( flags & TCP_SYN ) )
		flags |= TCP_PSH;
	tcphdr = iob_push ( iobuf, sizeof ( *tcphdr ) );
	memset ( tcphdr, 0, sizeof ( *tcphdr ) );
	tcphdr->src = tcp->local_port;
	tcphdr->dest = tcp->peer.st_port;
	tcphdr->seq = htonl ( tcp->snd_seq );
	tcphdr->ack = htonl ( tcp->rcv_ack );
	tcphdr->hlen = ( ( payload - iobuf->data ) << 2 );
	tcphdr->flags = flags;
	tcphdr->win = htons ( tcp->rcv_win );
	tcphdr->csum = tcpip_chksum ( iobuf->data, iob_len ( iobuf ) );

	/* Dump header */
	DBGC2 ( tcp, "TCP %p TX %d->%d %08x..%08x           %08x %4zd",
		tcp, ntohs ( tcphdr->src ), ntohs ( tcphdr->dest ),
		ntohl ( tcphdr->seq ), ( ntohl ( tcphdr->seq ) + seq_len ),
		ntohl ( tcphdr->ack ), len );
	tcp_dump_flags ( tcp, tcphdr->flags );
	DBGC2 ( tcp, "\n" );

	/* Transmit packet */
	if ( ( rc = tcpip_tx ( iobuf, &tcp_protocol, NULL, &tcp->peer, NULL,
			       &tcphdr->csum ) ) != 0 ) {
		DBGC ( tcp, "TCP %p could not transmit %08x..%08x %08x: %s\n",
		       tcp, tcp->snd_seq, ( tcp->snd_seq + tcp->snd_sent ),
		       tcp->rcv_ack, strerror ( rc ) );
		return rc;
	}

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

	DBGC ( tcp, "TCP %p timer %s in %s for %08x..%08x %08x\n", tcp,
	       ( over ? "expired" : "fired" ), tcp_state ( tcp->tcp_state ),
	       tcp->snd_seq, ( tcp->snd_seq + tcp->snd_sent ), tcp->rcv_ack );

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
		tcp->tcp_state = TCP_CLOSED;
		tcp_dump_state ( tcp );
		tcp_close ( tcp, -ETIMEDOUT );
	} else {
		/* Otherwise, retransmit the packet */
		tcp_xmit ( tcp, 0 );
	}
}

/**
 * Send RST response to incoming packet
 *
 * @v in_tcphdr		TCP header of incoming packet
 * @ret rc		Return status code
 */
static int tcp_xmit_reset ( struct tcp_connection *tcp,
			    struct sockaddr_tcpip *st_dest,
			    struct tcp_header *in_tcphdr ) {
	struct io_buffer *iobuf;
	struct tcp_header *tcphdr;
	int rc;

	/* Allocate space for dataless TX buffer */
	iobuf = alloc_iob ( MAX_HDR_LEN );
	if ( ! iobuf ) {
		DBGC ( tcp, "TCP %p could not allocate iobuf for RST "
		       "%08x..%08x %08x\n", tcp, ntohl ( in_tcphdr->ack ),
		       ntohl ( in_tcphdr->ack ), ntohl ( in_tcphdr->seq ) );
		return -ENOMEM;
	}
	iob_reserve ( iobuf, MAX_HDR_LEN );

	/* Construct RST response */
	tcphdr = iob_push ( iobuf, sizeof ( *tcphdr ) );
	memset ( tcphdr, 0, sizeof ( *tcphdr ) );
	tcphdr->src = in_tcphdr->dest;
	tcphdr->dest = in_tcphdr->src;
	tcphdr->seq = in_tcphdr->ack;
	tcphdr->ack = in_tcphdr->seq;
	tcphdr->hlen = ( ( sizeof ( *tcphdr ) / 4 ) << 4 );
	tcphdr->flags = ( TCP_RST | TCP_ACK );
	tcphdr->win = htons ( TCP_MAX_WINDOW_SIZE );
	tcphdr->csum = tcpip_chksum ( iobuf->data, iob_len ( iobuf ) );

	/* Dump header */
	DBGC2 ( tcp, "TCP %p TX %d->%d %08x..%08x           %08x %4d",
		tcp, ntohs ( tcphdr->src ), ntohs ( tcphdr->dest ),
		ntohl ( tcphdr->seq ), ( ntohl ( tcphdr->seq ) ),
		ntohl ( tcphdr->ack ), 0 );
	tcp_dump_flags ( tcp, tcphdr->flags );
	DBGC2 ( tcp, "\n" );

	/* Transmit packet */
	if ( ( rc = tcpip_tx ( iobuf, &tcp_protocol, NULL, st_dest,
			       NULL, &tcphdr->csum ) ) != 0 ) {
		DBGC ( tcp, "TCP %p could not transmit RST %08x..%08x %08x: "
		       "%s\n", tcp, ntohl ( in_tcphdr->ack ),
		       ntohl ( in_tcphdr->ack ), ntohl ( in_tcphdr->seq ),
		       strerror ( rc ) );
		return rc;
	}

	return 0;
}

/***************************************************************************
 *
 * Receive data path
 *
 ***************************************************************************
 */

/**
 * Identify TCP connection by local port number
 *
 * @v local_port	Local port (in network-endian order)
 * @ret tcp		TCP connection, or NULL
 */
static struct tcp_connection * tcp_demux ( unsigned int local_port ) {
	struct tcp_connection *tcp;

	list_for_each_entry ( tcp, &tcp_conns, list ) {
		if ( tcp->local_port == local_port )
			return tcp;
	}
	return NULL;
}

/**
 * Parse TCP received options
 *
 * @v tcp		TCP connection
 * @v data		Raw options data
 * @v len		Raw options length
 * @v options		Options structure to fill in
 */
static void tcp_rx_opts ( struct tcp_connection *tcp, const void *data,
			  size_t len, struct tcp_options *options ) {
	const void *end = ( data + len );
	const struct tcp_option *option;
	unsigned int kind;

	memset ( options, 0, sizeof ( *options ) );
	while ( data < end ) {
		option = data;
		kind = option->kind;
		if ( kind == TCP_OPTION_END )
			return;
		if ( kind == TCP_OPTION_NOP ) {
			data++;
			continue;
		}
		switch ( kind ) {
		case TCP_OPTION_MSS:
			options->mssopt = data;
			break;
		case TCP_OPTION_TS:
			options->tsopt = data;
			break;
		default:
			DBGC ( tcp, "TCP %p received unknown option %d\n",
			       tcp, kind );
			break;
		}
		data += option->length;
	}
}

/**
 * Consume received sequence space
 *
 * @v tcp		TCP connection
 * @v seq_len		Sequence space length to consume
 */
static void tcp_rx_seq ( struct tcp_connection *tcp, uint32_t seq_len ) {
	tcp->rcv_ack += seq_len;
	if ( tcp->rcv_win > seq_len ) {
		tcp->rcv_win -= seq_len;
	} else {
		tcp->rcv_win = 0;
	}
}

/**
 * Handle TCP received SYN
 *
 * @v tcp		TCP connection
 * @v seq		SEQ value (in host-endian order)
 * @v options		TCP options
 * @ret rc		Return status code
 */
static int tcp_rx_syn ( struct tcp_connection *tcp, uint32_t seq,
			struct tcp_options *options ) {

	/* Synchronise sequence numbers on first SYN */
	if ( ! ( tcp->tcp_state & TCP_STATE_RCVD ( TCP_SYN ) ) ) {
		tcp->rcv_ack = seq;
		if ( options->tsopt )
			tcp->timestamps = 1;
	}

	/* Ignore duplicate SYN */
	if ( ( tcp->rcv_ack - seq ) > 0 )
		return 0;

	/* Mark SYN as received and start sending ACKs with each packet */
	tcp->tcp_state |= ( TCP_STATE_SENT ( TCP_ACK ) |
			    TCP_STATE_RCVD ( TCP_SYN ) );

	/* Acknowledge SYN */
	tcp_rx_seq ( tcp, 1 );

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
	uint32_t ack_len = ( ack - tcp->snd_seq );
	size_t len;
	unsigned int acked_flags;

	/* Check for out-of-range or old duplicate ACKs */
	if ( ack_len > tcp->snd_sent ) {
		DBGC ( tcp, "TCP %p received ACK for %08x..%08x, "
		       "sent only %08x..%08x\n", tcp, tcp->snd_seq,
		       ( tcp->snd_seq + ack_len ), tcp->snd_seq,
		       ( tcp->snd_seq + tcp->snd_sent ) );

		if ( TCP_HAS_BEEN_ESTABLISHED ( tcp->tcp_state ) ) {
			/* Just ignore what might be old duplicate ACKs */
			return 0;
		} else {
			/* Send RST if an out-of-range ACK is received
			 * on a not-yet-established connection, as per
			 * RFC 793.
			 */
			return -EINVAL;
		}
	}

	/* Ignore ACKs that don't actually acknowledge any new data.
	 * (In particular, do not stop the retransmission timer; this
	 * avoids creating a sorceror's apprentice syndrome when a
	 * duplicate ACK is received and we still have data in our
	 * transmit queue.)
	 */
	if ( ack_len == 0 )
		return 0;

	/* Stop the retransmission timer */
	stop_timer ( &tcp->timer );

	/* Determine acknowledged flags and data length */
	len = ack_len;
	acked_flags = ( TCP_FLAGS_SENDING ( tcp->tcp_state ) &
			( TCP_SYN | TCP_FIN ) );
	if ( acked_flags )
		len--;

	/* Update SEQ and sent counters, and window size */
	tcp->snd_seq = ack;
	tcp->snd_sent = 0;
	tcp->snd_win = win;

	/* Remove any acknowledged data from transmit queue */
	tcp_process_queue ( tcp, len, NULL, 1 );
		
	/* Mark SYN/FIN as acknowledged if applicable. */
	if ( acked_flags )
		tcp->tcp_state |= TCP_STATE_ACKED ( acked_flags );

	/* Start sending FIN if we've had all possible data ACKed */
	if ( list_empty ( &tcp->queue ) && tcp->xfer_closed )
		tcp->tcp_state |= TCP_STATE_SENT ( TCP_FIN );

	return 0;
}

/**
 * Handle TCP received data
 *
 * @v tcp		TCP connection
 * @v seq		SEQ value (in host-endian order)
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 *
 * This function takes ownership of the I/O buffer.
 */
static int tcp_rx_data ( struct tcp_connection *tcp, uint32_t seq,
			 struct io_buffer *iobuf ) {
	uint32_t already_rcvd;
	uint32_t len;
	int rc;

	/* Ignore duplicate or out-of-order data */
	already_rcvd = ( tcp->rcv_ack - seq );
	len = iob_len ( iobuf );
	if ( already_rcvd >= len ) {
		free_iob ( iobuf );
		return 0;
	}
	iob_pull ( iobuf, already_rcvd );
	len -= already_rcvd;

	/* Deliver data to application */
	if ( ( rc = xfer_deliver_iob ( &tcp->xfer, iobuf ) ) != 0 ) {
		DBGC ( tcp, "TCP %p could not deliver %08x..%08x: %s\n",
		       tcp, seq, ( seq + len ), strerror ( rc ) );
		return rc;
	}

	/* Acknowledge new data */
	tcp_rx_seq ( tcp, len );

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

	/* Ignore duplicate or out-of-order FIN */
	if ( ( tcp->rcv_ack - seq ) > 0 )
		return 0;

	/* Mark FIN as received and acknowledge it */
	tcp->tcp_state |= TCP_STATE_RCVD ( TCP_FIN );
	tcp_rx_seq ( tcp, 1 );

	/* Close connection */
	tcp_close ( tcp, 0 );

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
		if ( ( seq - tcp->rcv_ack ) >= tcp->rcv_win )
			return 0;
	} else {
		if ( ! ( tcp->tcp_state & TCP_STATE_ACKED ( TCP_SYN ) ) )
			return 0;
	}

	/* Abort connection */
	tcp->tcp_state = TCP_CLOSED;
	tcp_dump_state ( tcp );
	tcp_close ( tcp, -ECONNRESET );

	DBGC ( tcp, "TCP %p connection reset by peer\n", tcp );
	return -ECONNRESET;
}

/**
 * Process received packet
 *
 * @v iobuf		I/O buffer
 * @v st_src		Partially-filled source address
 * @v st_dest		Partially-filled destination address
 * @v pshdr_csum	Pseudo-header checksum
 * @ret rc		Return status code
  */
static int tcp_rx ( struct io_buffer *iobuf,
		    struct sockaddr_tcpip *st_src,
		    struct sockaddr_tcpip *st_dest __unused,
		    uint16_t pshdr_csum ) {
	struct tcp_header *tcphdr = iobuf->data;
	struct tcp_connection *tcp;
	struct tcp_options options;
	size_t hlen;
	uint16_t csum;
	uint32_t start_seq;
	uint32_t seq;
	uint32_t ack;
	uint32_t win;
	unsigned int flags;
	size_t len;
	int rc;

	/* Sanity check packet */
	if ( iob_len ( iobuf ) < sizeof ( *tcphdr ) ) {
		DBG ( "TCP packet too short at %zd bytes (min %zd bytes)\n",
		      iob_len ( iobuf ), sizeof ( *tcphdr ) );
		rc = -EINVAL;
		goto discard;
	}
	hlen = ( ( tcphdr->hlen & TCP_MASK_HLEN ) / 16 ) * 4;
	if ( hlen < sizeof ( *tcphdr ) ) {
		DBG ( "TCP header too short at %zd bytes (min %zd bytes)\n",
		      hlen, sizeof ( *tcphdr ) );
		rc = -EINVAL;
		goto discard;
	}
	if ( hlen > iob_len ( iobuf ) ) {
		DBG ( "TCP header too long at %zd bytes (max %zd bytes)\n",
		      hlen, iob_len ( iobuf ) );
		rc = -EINVAL;
		goto discard;
	}
	csum = tcpip_continue_chksum ( pshdr_csum, iobuf->data,
				       iob_len ( iobuf ) );
	if ( csum != 0 ) {
		DBG ( "TCP checksum incorrect (is %04x including checksum "
		      "field, should be 0000)\n", csum );
		rc = -EINVAL;
		goto discard;
	}
	
	/* Parse parameters from header and strip header */
	tcp = tcp_demux ( tcphdr->dest );
	start_seq = seq = ntohl ( tcphdr->seq );
	ack = ntohl ( tcphdr->ack );
	win = ntohs ( tcphdr->win );
	flags = tcphdr->flags;
	tcp_rx_opts ( tcp, ( ( ( void * ) tcphdr ) + sizeof ( *tcphdr ) ),
		      ( hlen - sizeof ( *tcphdr ) ), &options );
	iob_pull ( iobuf, hlen );
	len = iob_len ( iobuf );

	/* Dump header */
	DBGC2 ( tcp, "TCP %p RX %d<-%d           %08x %08x..%08zx %4zd",
		tcp, ntohs ( tcphdr->dest ), ntohs ( tcphdr->src ),
		ntohl ( tcphdr->ack ), ntohl ( tcphdr->seq ),
		( ntohl ( tcphdr->seq ) + len +
		  ( ( tcphdr->flags & ( TCP_SYN | TCP_FIN ) ) ? 1 : 0 )), len);
	tcp_dump_flags ( tcp, tcphdr->flags );
	DBGC2 ( tcp, "\n" );

	/* If no connection was found, send RST */
	if ( ! tcp ) {
		tcp_xmit_reset ( tcp, st_src, tcphdr );
		rc = -ENOTCONN;
		goto discard;
	}

	/* Handle ACK, if present */
	if ( flags & TCP_ACK ) {
		if ( ( rc = tcp_rx_ack ( tcp, ack, win ) ) != 0 ) {
			tcp_xmit_reset ( tcp, st_src, tcphdr );
			goto discard;
		}
	}

	/* Handle SYN, if present */
	if ( flags & TCP_SYN ) {
		tcp_rx_syn ( tcp, seq, &options );
		seq++;
	}

	/* Handle RST, if present */
	if ( flags & TCP_RST ) {
		if ( ( rc = tcp_rx_rst ( tcp, seq ) ) != 0 )
			goto discard;
	}

	/* Handle new data, if any */
	tcp_rx_data ( tcp, seq, iobuf );
	seq += len;

	/* Handle FIN, if present */
	if ( flags & TCP_FIN ) {
		tcp_rx_fin ( tcp, seq );
		seq++;
	}

	/* Update timestamp, if present and applicable */
	if ( ( seq == tcp->rcv_ack ) && options.tsopt )
		tcp->ts_recent = ntohl ( options.tsopt->tsval );

	/* Dump out any state change as a result of the received packet */
	tcp_dump_state ( tcp );

	/* Send out any pending data.  We force sending a reply if either
	 *
	 *  a) the peer is expecting an ACK (i.e. consumed sequence space), or
	 *  b) either end of the packet was outside the receive window
	 *
	 * Case (b) enables us to support TCP keepalives using
	 * zero-length packets, which we would otherwise ignore.  Note
	 * that for case (b), we need *only* consider zero-length
	 * packets, since non-zero-length packets will already be
	 * caught by case (a).
	 */
	tcp_xmit ( tcp, ( ( start_seq != seq ) ||
			  ( ( seq - tcp->rcv_ack ) > tcp->rcv_win ) ) );

	/* If this packet was the last we expect to receive, set up
	 * timer to expire and cause the connection to be freed.
	 */
	if ( TCP_CLOSED_GRACEFULLY ( tcp->tcp_state ) ) {
		tcp->timer.timeout = ( 2 * TCP_MSL );
		start_timer ( &tcp->timer );
	}

	return 0;

 discard:
	/* Free received packet */
	free_iob ( iobuf );
	return rc;
}

/** TCP protocol */
struct tcpip_protocol tcp_protocol __tcpip_protocol = {
	.name = "TCP",
	.rx = tcp_rx,
	.tcpip_proto = IP_TCP,
};

/***************************************************************************
 *
 * Data transfer interface
 *
 ***************************************************************************
 */

/**
 * Close interface
 *
 * @v xfer		Data transfer interface
 * @v rc		Reason for close
 */
static void tcp_xfer_close ( struct xfer_interface *xfer, int rc ) {
	struct tcp_connection *tcp =
		container_of ( xfer, struct tcp_connection, xfer );

	/* Close data transfer interface */
	tcp_close ( tcp, rc );

	/* Transmit FIN, if possible */
	tcp_xmit ( tcp, 0 );
}

/**
 * Check flow control window
 *
 * @v xfer		Data transfer interface
 * @ret len		Length of window
 */
static size_t tcp_xfer_window ( struct xfer_interface *xfer ) {
	struct tcp_connection *tcp =
		container_of ( xfer, struct tcp_connection, xfer );

	/* Not ready if data queue is non-empty.  This imposes a limit
	 * of only one unACKed packet in the TX queue at any time; we
	 * do this to conserve memory usage.
	 */
	if ( ! list_empty ( &tcp->queue ) )
		return 0;

	/* Return TCP window length */
	return tcp_xmit_win ( tcp );
}

/**
 * Deliver datagram as I/O buffer
 *
 * @v xfer		Data transfer interface
 * @v iobuf		Datagram I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int tcp_xfer_deliver_iob ( struct xfer_interface *xfer,
				  struct io_buffer *iobuf,
				  struct xfer_metadata *meta __unused ) {
	struct tcp_connection *tcp =
		container_of ( xfer, struct tcp_connection, xfer );

	/* Enqueue packet */
	list_add_tail ( &iobuf->list, &tcp->queue );

	/* Transmit data, if possible */
	tcp_xmit ( tcp, 0 );

	return 0;
}

/** TCP data transfer interface operations */
static struct xfer_interface_operations tcp_xfer_operations = {
	.close		= tcp_xfer_close,
	.vredirect	= ignore_xfer_vredirect,
	.window		= tcp_xfer_window,
	.alloc_iob	= default_xfer_alloc_iob,
	.deliver_iob	= tcp_xfer_deliver_iob,
	.deliver_raw	= xfer_deliver_as_iob,
};

/***************************************************************************
 *
 * Openers
 *
 ***************************************************************************
 */

/** TCP socket opener */
struct socket_opener tcp_socket_opener __socket_opener = {
	.semantics	= TCP_SOCK_STREAM,
	.family		= AF_INET,
	.open		= tcp_open,
};

/** Linkage hack */
int tcp_sock_stream = TCP_SOCK_STREAM;

/**
 * Open TCP URI
 *
 * @v xfer		Data transfer interface
 * @v uri		URI
 * @ret rc		Return status code
 */
static int tcp_open_uri ( struct xfer_interface *xfer, struct uri *uri ) {
	struct sockaddr_tcpip peer;

	/* Sanity check */
	if ( ! uri->host )
		return -EINVAL;

	memset ( &peer, 0, sizeof ( peer ) );
	peer.st_port = htons ( uri_port ( uri, 0 ) );
	return xfer_open_named_socket ( xfer, SOCK_STREAM,
					( struct sockaddr * ) &peer,
					uri->host, NULL );
}

/** TCP URI opener */
struct uri_opener tcp_uri_opener __uri_opener = {
	.scheme		= "tcp",
	.open		= tcp_open_uri,
};

