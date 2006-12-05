#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <byteswap.h>
#include <latch.h>
#include <errno.h>
#include <gpxe/process.h>
#include <gpxe/init.h>
#include <gpxe/netdevice.h>
#include <gpxe/pkbuff.h>
#include <gpxe/ip.h>
#include <gpxe/tcp.h>
#include <gpxe/tcpip.h>
#include <gpxe/retry.h>
#include "uip/uip.h"

/** @file
 *
 * TCP protocol
 *
 * The gPXE TCP stack is currently implemented on top of the uIP
 * protocol stack.  This file provides wrappers around uIP so that
 * higher-level protocol implementations do not need to talk directly
 * to uIP (which has a somewhat baroque API).
 *
 * Basic operation is to create a #tcp_connection structure, call
 * tcp_connect() and then call run_tcpip() in a loop until the
 * operation has completed.  The TCP stack will call the various
 * methods defined in the #tcp_operations structure in order to send
 * and receive data.
 *
 * See hello.c for a trivial example of a TCP protocol using this
 * API.
 *
 */

#if USE_UIP

/**
 * TCP transmit buffer
 *
 * When a tcp_operations::senddata() method is called, it is
 * guaranteed to be able to use this buffer as temporary space for
 * constructing the data to be sent.  For example, code such as
 *
 * @code
 *
 *     static void my_senddata ( struct tcp_connection *conn, void *buf,
 *				 size_t len ) {
 *         len = snprintf ( buf, len, "FETCH %s\r\n", filename );
 *         tcp_send ( conn, buf + already_sent, len - already_sent );
 *     }
 *
 * @endcode
 *
 * is allowed, and is probably the best way to deal with
 * variably-sized data.
 *
 * Note that you cannot use this simple mechanism if you want to be
 * able to construct single data blocks of more than #len bytes.
 */
static void *tcp_buffer = uip_buf + ( 40 + UIP_LLH_LEN );

/** Size of #tcp_buffer */
static size_t tcp_buflen = UIP_BUFSIZE - ( 40 + UIP_LLH_LEN );

/**
 * Open a TCP connection
 *
 * @v conn	TCP connection
 * 
 * This sets up a new TCP connection to the remote host specified in
 * tcp_connection::sin.
 */
void tcp_connect ( struct tcp_connection *conn ) {
	struct uip_conn *uip_conn;
	u16_t ipaddr[2];

	assert ( conn->sin.sin_addr.s_addr != 0 );
	assert ( conn->sin.sin_port != 0 );
	assert ( conn->tcp_op != NULL );
	assert ( sizeof ( uip_conn->appstate ) == sizeof ( conn ) );

	* ( ( uint32_t * ) ipaddr ) = conn->sin.sin_addr.s_addr;
	uip_conn = uip_connect ( ipaddr, conn->sin.sin_port );
#warning "Use linked lists so that uip_connect() cannot fail"
	assert ( uip_conn != NULL );
	*( ( void ** ) uip_conn->appstate ) = conn;
}

/**
 * Send data via a TCP connection
 *
 * @v conn	TCP connection
 * @v data	Data to send
 * @v len	Length of data
 *
 * Data will be automatically limited to the current TCP window size.
 *
 * If retransmission is required, the connection's
 * tcp_operations::senddata() method will be called again in order to
 * regenerate the data.
 */
void tcp_send ( struct tcp_connection *conn __unused,
		const void *data, size_t len ) {

	assert ( conn = *( ( void ** ) uip_conn->appstate ) );

	if ( len > tcp_buflen )
		len = tcp_buflen;
	memmove ( tcp_buffer, data, len );

	uip_send ( tcp_buffer, len );
}

/**
 * Close a TCP connection
 *
 * @v conn	TCP connection
 */
void tcp_close ( struct tcp_connection *conn __unused ) {
	assert ( conn = *( ( void ** ) uip_conn->appstate ) );
	uip_close();
}

/**
 * uIP TCP application call interface
 *
 * This is the entry point of gPXE from the point of view of the uIP
 * protocol stack.  This function calls the appropriate methods from
 * the connection's @tcp_operations table in order to process received
 * data, transmit new data etc.
 */
void uip_tcp_appcall ( void ) {
	struct tcp_connection *conn = *( ( void ** ) uip_conn->appstate );
	struct tcp_operations *op = conn->tcp_op;

	if ( op->closed ) {
		if ( uip_aborted() )
			op->closed ( conn, -ECONNABORTED );
		if ( uip_timedout() )
			op->closed ( conn, -ETIMEDOUT );
		if ( uip_closed() )
			op->closed ( conn, 0 );
	}
	if ( uip_connected() && op->connected )
		op->connected ( conn );
	if ( uip_acked() && op->acked )
		op->acked ( conn, uip_conn->len );
	if ( uip_newdata() && op->newdata )
		op->newdata ( conn, ( void * ) uip_appdata, uip_len );
	if ( ( uip_rexmit() || uip_newdata() || uip_acked() ||
	       uip_connected() || uip_poll() ) && op->senddata )
		op->senddata ( conn, tcp_buffer, tcp_buflen );
}

/* Present here to allow everything to link.  Will go into separate
 * udp.c file
 */
void uip_udp_appcall ( void ) {
}

/**
 * Perform periodic processing of all TCP connections
 *
 * This allows TCP connections to retransmit data if necessary.
 */
static void tcp_periodic ( void ) {
	struct pk_buff *pkb;
	int i;

	for ( i = 0 ; i < UIP_CONNS ; i++ ) {
		uip_periodic ( i );
		if ( uip_len > 0 ) {
			pkb = alloc_pkb ( uip_len + MAX_LL_HEADER_LEN);
			if ( ! pkb )
				continue;
				
			pkb_reserve ( pkb, MAX_LL_HEADER_LEN );
			pkb_put ( pkb, uip_len );
			memcpy ( pkb->data, uip_buf, uip_len );

			ipv4_uip_tx ( pkb );
		}
	}
}

/**
 * Kick a connection into life
 *
 * @v conn	TCP connection
 *
 * Call this function when you have new data to send and are not
 * already being called as part of TCP processing.
 */
void tcp_kick ( struct tcp_connection *conn __unused ) {
	/* Just kick all the connections; this will work for now */
	tcp_periodic();
}

/**
 * Single-step the TCP stack
 *
 * @v process	TCP process
 *
 * This calls tcp_periodic() at regular intervals.
 */
static void tcp_step ( struct process *process ) {
	static unsigned long timeout = 0;

	if ( currticks() > timeout ) {
		timeout = currticks() + ( TICKS_PER_SEC / 10 );
		tcp_periodic ();
	}

	schedule ( process );
}

/** TCP stack process */
static struct process tcp_process = {
	.step = tcp_step,
};

/** Initialise the TCP stack */
static void init_tcp ( void ) {
	schedule ( &tcp_process );
}

INIT_FN ( INIT_PROCESS, init_tcp, NULL, NULL );

#else

/**
 * List of registered TCP connections
 */
static LIST_HEAD ( tcp_conns );

/**
 * List of TCP states
 */
static const char *tcp_states[] = {
	"CLOSED",
	"LISTEN",
	"SYN_SENT",
	"SYN_RCVD",
	"ESTABLISHED",
	"FIN_WAIT_1",
	"FIN_WAIT_2",
	"CLOSING",
	"TIME_WAIT",
	"CLOSE_WAIT",
	"LAST_ACK",
	"INVALID" };

/**
 * TCP state transition function
 *
 * @v conn	TCP connection
 * @v nxt_state Next TCP state
 */
void tcp_set_flags ( struct tcp_connection *conn ) {

	/* Set the TCP flags */
	switch ( conn->tcp_state ) {
	case TCP_CLOSED:
		if ( conn->tcp_lstate == TCP_SYN_RCVD ) {
			conn->tcp_flags |= TCP_RST;
		}
		break;
	case TCP_LISTEN:
		break;
	case TCP_SYN_SENT:
		if ( conn->tcp_lstate == TCP_LISTEN ||
		     conn->tcp_lstate == TCP_CLOSED ) {
			conn->tcp_flags |= TCP_SYN;
		}
		break;
	case TCP_SYN_RCVD:
		if ( conn->tcp_lstate == TCP_LISTEN ||
		     conn->tcp_lstate == TCP_SYN_SENT ) {
			conn->tcp_flags |= ( TCP_SYN | TCP_ACK );
		}
		break;
	case TCP_ESTABLISHED:
		if ( conn->tcp_lstate == TCP_SYN_SENT ) {
			conn->tcp_flags |= TCP_ACK;
		}
		break;
	case TCP_FIN_WAIT_1:
		if ( conn->tcp_lstate == TCP_SYN_RCVD ||
		     conn->tcp_lstate == TCP_ESTABLISHED ) {
			conn->tcp_flags |= TCP_FIN;
		}
		break;
	case TCP_FIN_WAIT_2:
		break;
	case TCP_CLOSING:
		if ( conn->tcp_lstate == TCP_FIN_WAIT_1 ) {
			conn->tcp_flags |= TCP_ACK;
		}
		break;
	case TCP_TIME_WAIT:
		if ( conn->tcp_lstate == TCP_FIN_WAIT_1 ||
		     conn->tcp_lstate == TCP_FIN_WAIT_2 ) {
			conn->tcp_flags |= TCP_ACK;
		}
		break;
	case TCP_CLOSE_WAIT:
		if ( conn->tcp_lstate == TCP_ESTABLISHED ) {
			conn->tcp_flags |= TCP_ACK;
		}
		break;
	case TCP_LAST_ACK:
		if ( conn->tcp_lstate == TCP_CLOSE_WAIT ) {
			conn->tcp_flags |= TCP_FIN;
		}
		if ( conn->tcp_lstate == TCP_ESTABLISHED ) {
			conn->tcp_flags |= ( TCP_FIN | TCP_ACK );
		}
		break;
	default:
		DBG ( "TCP_INVALID state %d\n", conn->tcp_state );
		return;
	}
}

void tcp_trans ( struct tcp_connection *conn, int nxt_state ) {
	/* Remember the last state */
	conn->tcp_lstate = conn->tcp_state;
	conn->tcp_state = nxt_state;

	DBG ( "Transition from %s to %s\n", tcp_states[conn->tcp_lstate], tcp_states[conn->tcp_state] );

	/* TODO: Check if this check is required */
	if ( conn->tcp_lstate == conn->tcp_state || 
	     conn->tcp_state == TCP_INVALID ) {
		conn->tcp_flags = 0;
		return;
	}
	tcp_set_flags ( conn );
}

/**
 * Dump TCP header
 *
 * @v tcphdr	TCP header
 */
void tcp_dump ( struct tcp_header *tcphdr ) {
	DBG ( "TCP %p src:%d dest:%d seq:%lx ack:%lx hlen:%hd flags:%#hx\n",
		tcphdr, ntohs ( tcphdr->src ), ntohs ( tcphdr->dest ), ntohl ( tcphdr->seq ),
		ntohl ( tcphdr->ack ), ( ( tcphdr->hlen & TCP_MASK_HLEN ) / 16 ), ( tcphdr->flags & TCP_MASK_FLAGS ) );
}

/**
 * Initialize a TCP connection
 *
 * @v conn	TCP connection
 *
 * This function assigns initial values to some fields in the connection
 * structure. The application should call tcp_init_conn after creating a new
 * connection before calling any other "tcp_*" function.
 *
 * struct tcp_connection my_conn;
 * tcp_init_conn ( &my_conn );
 * ... 
 */
void tcp_init_conn ( struct tcp_connection *conn ) {
	conn->local_port = 0;
	conn->tcp_state = TCP_CLOSED;
	conn->tcp_lstate = TCP_INVALID;
	conn->tx_pkb = NULL;
	conn->tcp_op = NULL;
}

/** Retry timer
 *
 * @v timer	Retry timer
 * @v over	Failure indicator
 */
void tcp_expired ( struct retry_timer *timer, int over ) {
	struct tcp_connection *conn;
	conn = ( struct tcp_connection * ) container_of ( timer, 
					struct tcp_connection, timer );
	DBG ( "Timer expired in %s\n", tcp_states[conn->tcp_state] );
	switch ( conn->tcp_state ) {
	case TCP_SYN_SENT:
		if ( over ) {
			tcp_trans ( conn, TCP_CLOSED );
			DBG ( "Timeout! Connection closed\n" );
			return;
		}
		goto send_tcp_nomsg;
	case TCP_SYN_RCVD:
		if ( over ) {
			tcp_trans ( conn, TCP_CLOSED );
			goto send_tcp_nomsg;
		}
		goto send_tcp_nomsg;
	case TCP_ESTABLISHED:
		if ( conn->tcp_lstate == TCP_SYN_SENT ) {
			goto send_tcp_nomsg;
		}
		break;
	case TCP_CLOSE_WAIT:
		if ( conn->tcp_lstate == TCP_ESTABLISHED ) {
			goto send_tcp_nomsg;
		}
		break;
	case TCP_FIN_WAIT_1:
	case TCP_FIN_WAIT_2:
		goto send_tcp_nomsg;
	case TCP_CLOSING:
	case TCP_LAST_ACK:
		if ( conn->tcp_lstate == TCP_CLOSE_WAIT ) {
			goto send_tcp_nomsg;
		}
		return;
	case TCP_TIME_WAIT:
		tcp_trans ( conn, TCP_CLOSED );
		return;
	}
	/* Retransmit the data */
	tcp_set_flags ( conn );
	tcp_senddata ( conn );
	return;

  send_tcp_nomsg:
	free_pkb ( conn->tx_pkb );
	conn->tx_pkb = alloc_pkb ( MIN_PKB_LEN );
	pkb_reserve ( conn->tx_pkb, MAX_HDR_LEN );
	tcp_set_flags ( conn );
	int rc;
	if ( ( rc = tcp_send ( conn, TCP_NOMSG, TCP_NOMSG_LEN ) ) != 0 ) {
		DBG ( "Error sending TCP message (rc = %d)\n", rc );
	}
	return;
}

/**
 * Connect to a remote server
 *
 * @v conn	TCP connection
 * @v peer	Remote socket address
 *
 * This function initiates a TCP connection to the socket address specified in
 * peer. It sends a SYN packet to peer. When the connection is established, the
 * TCP stack calls the connected() callback function.
 */
int tcp_connectto ( struct tcp_connection *conn,
		    struct sockaddr_tcpip *peer ) {
	int rc;

	/* A connection can only be established from the CLOSED state */
	if ( conn->tcp_state != TCP_CLOSED ) {
		DBG ( "Error opening connection: Invalid state %s\n",
				tcp_states[conn->tcp_state] );
		return -EISCONN;
	}

	/* Add the connection to the set of listening connections */
	if ( ( rc = tcp_listen ( conn, conn->local_port ) ) != 0 ) {
		return rc;
	}
	memcpy ( &conn->peer, peer, sizeof ( conn->peer ) );

	/* Initialize the TCP timer */
	conn->timer.expired = tcp_expired;

	/* Send a SYN packet and transition to TCP_SYN_SENT */
	conn->snd_una = random();
	tcp_trans ( conn, TCP_SYN_SENT );
	/* Allocate space for the packet */
	free_pkb ( conn->tx_pkb );
	conn->tx_pkb = alloc_pkb ( MIN_PKB_LEN );
	pkb_reserve ( conn->tx_pkb, MAX_HDR_LEN );
	conn->rcv_win = MAX_PKB_LEN - MAX_HDR_LEN; /* TODO: Is this OK? */
	return tcp_send ( conn, TCP_NOMSG, TCP_NOMSG_LEN );
}

int tcp_connect ( struct tcp_connection *conn ) {
	return tcp_connectto ( conn, &conn->peer );
}

/**
 * Close the connection
 *
 * @v conn
 *
 * This function sends a FIN packet to the remote end of the connection. When
 * the remote end of the connection ACKs the FIN (FIN consumes one byte on the
 * snd stream), the stack invokes the closed() callback function.
 */
int tcp_close ( struct tcp_connection *conn ) {
	/* A connection can only be closed if it is a connected state */
	switch ( conn->tcp_state ) {
	case TCP_SYN_RCVD:
	case TCP_ESTABLISHED:
		tcp_trans ( conn, TCP_FIN_WAIT_1 );
		if ( conn->tcp_op->closed )
			conn->tcp_op->closed ( conn, CONN_SNDCLOSE ); /* TODO: Check! */
		/* FIN consumes one byte on the snd stream */
//		conn->snd_una++;
		goto send_tcp_nomsg;
	case TCP_SYN_SENT:
	case TCP_LISTEN:
		/**
		 * Since the connection does not expect any packets from the
		 * remote end, it can be removed from the set of listening
		 * connections.
		 */
		list_del ( &conn->list );
		tcp_trans ( conn, TCP_CLOSED );
		if ( conn->tcp_op->closed )
			conn->tcp_op->closed ( conn, CONN_SNDCLOSE );
		return 0;
	case TCP_CLOSE_WAIT:
		tcp_trans ( conn, TCP_LAST_ACK );
		if ( conn->tcp_op->closed )
			conn->tcp_op->closed ( conn, CONN_SNDCLOSE ); /* TODO: Check! */
		/* FIN consumes one byte on the snd stream */
//		conn->snd_una++;
		goto send_tcp_nomsg;
	default:
		DBG ( "tcp_close(): Invalid state %s\n",
					tcp_states[conn->tcp_state] );
		return -EPROTO;
	}

  send_tcp_nomsg:
	free_pkb ( conn->tx_pkb );
	conn->tx_pkb = alloc_pkb ( MIN_PKB_LEN );
	conn->tcp_flags = TCP_FIN;
	pkb_reserve ( conn->tx_pkb, MAX_HDR_LEN );
	return tcp_send ( conn, TCP_NOMSG, TCP_NOMSG_LEN );
}

/**
 * Bind TCP connection to local port
 *
 * @v conn		TCP connection
 * @v local_port	Local port, in network byte order
 * @ret rc		Return status code
 */
int tcp_bind ( struct tcp_connection *conn, uint16_t local_port ) {
	struct tcp_connection *existing;

	list_for_each_entry ( existing, &tcp_conns, list ) {
		if ( existing->local_port == local_port )
			return -EADDRINUSE;
	}
	conn->local_port = local_port;
	return 0;
}


/**
 * Listen for a packet
 *
 * @v conn		TCP connection
 * @v local_port	Local port, in network byte order
 *
 * This function adds the connection to a list of registered tcp
 * connections. If the local port is 0, the connection is assigned an
 * available port between MIN_TCP_PORT and 65535.
 */
int tcp_listen ( struct tcp_connection *conn, uint16_t local_port ) {
	static uint16_t try_port = 1024;
	int rc;

#warning "Fix the port re-use bug"
	/* If we re-use the same port, the connection should be reset
	 * and a new connection set up.  This doesn't happen yet, so
	 * randomise the port to avoid hitting the problem.
	 */
	try_port = random();

	/* If no port specified, find the first available port */
	if ( ! local_port ) {
		for ( ; try_port ; try_port++ ) {
			if ( try_port < 1024 )
				continue;
			if ( tcp_listen ( conn, htons ( try_port ) ) == 0 )
				return 0;
		}
		return -EADDRINUSE;
	}

	/* Attempt bind to local port */
	if ( ( rc = tcp_bind ( conn, local_port ) ) != 0 )
		return rc;

	/* Add to TCP connection list */
	list_add ( &conn->list, &tcp_conns );
	DBG ( "TCP opened %p on port %d\n", conn, ntohs ( local_port ) );

	return 0;
}

/**
 * Send data
 *
 * @v conn	TCP connection
 * 
 * This function allocates space to the transmit buffer and invokes the
 * senddata() callback function. It passes the allocated buffer to senddata().
 * The applicaion may use this space to write it's data.
 */
int tcp_senddata ( struct tcp_connection *conn ) {
	/* The connection must be in a state in which the user can send data */
	switch ( conn->tcp_state ) {
	case TCP_LISTEN:
		tcp_trans ( conn, TCP_SYN_SENT );
		conn->snd_una = random();
		break;
	case TCP_ESTABLISHED:
	case TCP_CLOSE_WAIT:
		break;
	default:
		DBG ( "tcp_senddata: Invalid state %s\n",
				tcp_states[conn->tcp_state] );
		return -EPROTO;
	}

	/* Allocate space to the TX buffer */
	free_pkb ( conn->tx_pkb );
	conn->tx_pkb = alloc_pkb ( MAX_PKB_LEN );
	if ( !conn->tx_pkb ) {
		DBG ( "Insufficient memory\n" );
		return -ENOMEM;
	}
	pkb_reserve ( conn->tx_pkb, MAX_HDR_LEN );
	/* Set the advertised window */
	conn->rcv_win = pkb_available ( conn->tx_pkb );
	/* Call the senddata() call back function */
	if ( conn->tcp_op->senddata )
		conn->tcp_op->senddata ( conn, conn->tx_pkb->data, 
					 pkb_available ( conn->tx_pkb ) );
	/* Send pure ACK if senddata() didn't call tcp_send() */
	if ( conn->tx_pkb ) {
		tcp_send ( conn, TCP_NOMSG, TCP_NOMSG_LEN );
	}
	return 0;
}

/**
 * Transmit data
 *
 * @v conn	TCP connection
 * @v data	Data to be sent
 * @v len	Length of the data
 *
 * This function sends data to the peer socket address
 */
int tcp_send ( struct tcp_connection *conn, const void *data, size_t len ) {
	struct sockaddr_tcpip *peer = &conn->peer;
	struct pk_buff *pkb;
	int slen;

	/* Take ownership of the TX buffer from the connection */
	pkb = conn->tx_pkb;
	conn->tx_pkb = NULL;

	/* Determine the amount of data to be sent */
	slen = len < conn->snd_win ? len : conn->snd_win;
	/* Copy payload */
	memmove ( pkb_put ( pkb, slen ), data, slen );

	/* Fill up the TCP header */
	struct tcp_header *tcphdr = pkb_push ( pkb, sizeof ( *tcphdr ) );

	/* Source port, assumed to be in network byte order in conn */
	tcphdr->src = conn->local_port;
	/* Destination port, assumed to be in network byte order in peer */
	tcphdr->dest = peer->st_port;
	tcphdr->seq = htonl ( conn->snd_una );
	tcphdr->ack = htonl ( conn->rcv_nxt );
	/* Header length, = 0x50 (without TCP options) */
	tcphdr->hlen = ( uint8_t ) ( ( sizeof ( *tcphdr ) / 4 ) << 4 );
	/* Copy TCP flags, and then reset the variable */
	tcphdr->flags = conn->tcp_flags;
	conn->tcp_flags = 0;
	/* Advertised window, in network byte order */
	tcphdr->win = htons ( conn->rcv_win );
	/* Set urgent pointer to 0 */
	tcphdr->urg = 0;
	/* Calculate and store partial checksum, in host byte order */
	tcphdr->csum = 0;
	tcphdr->csum = tcpip_chksum ( pkb->data, pkb_len ( pkb ) );
	
	/* Dump the TCP header */
	tcp_dump ( tcphdr );

	/* Start the timer */
	if ( ( conn->tcp_state == TCP_ESTABLISHED && conn->tcp_lstate == TCP_SYN_SENT ) ||
	     ( conn->tcp_state == TCP_LISTEN && conn->tcp_lstate == TCP_SYN_RCVD ) ||
	     ( conn->tcp_state == TCP_CLOSED && conn->tcp_lstate == TCP_SYN_RCVD ) ||
	     ( conn->tcp_state == TCP_ESTABLISHED && ( len == 0 ) ) ) {
		// Don't start the timer
	} else {
		start_timer ( &conn->timer );
	}

	/* Transmit packet */
	return tcpip_tx ( pkb, &tcp_protocol, peer );
}

/**
 * Process received packet
 *
 * @v pkb	Packet buffer
 * @v partial	Partial checksum
 */
static int tcp_rx ( struct pk_buff *pkb,
		    struct sockaddr_tcpip *st_src __unused,
		    struct sockaddr_tcpip *st_dest __unused ) {
	struct tcp_connection *conn;
	struct tcp_header *tcphdr;
	uint32_t acked, toack;
	int hlen;
	int rc;

	/* Sanity check */
	if ( pkb_len ( pkb ) < sizeof ( *tcphdr ) ) {
		DBG ( "Packet too short (%d bytes)\n", pkb_len ( pkb ) );
		rc = -EINVAL;
		goto done;
	}

	/* Process TCP header */
	tcphdr = pkb->data;
	tcp_dump ( tcphdr );

	/* Verify header length */
	hlen = ( ( tcphdr->hlen & TCP_MASK_HLEN ) / 16 ) * 4;
	if ( hlen < sizeof ( *tcphdr ) ) {
		DBG ( "Bad header length (%d bytes)\n", hlen );
		rc = -EINVAL;
		goto done;
	}
	/* TODO: Parse TCP options */
	if ( hlen != sizeof ( *tcphdr ) ) {
		DBG ( "Ignoring TCP options\n" );
	}

	/* TODO: Verify checksum */
	
	/* Demux TCP connection */
	list_for_each_entry ( conn, &tcp_conns, list ) {
		if ( tcphdr->dest == conn->local_port ) {
			goto found_conn;
		}
	}
	
	DBG ( "No connection found on port %d\n", ntohs ( tcphdr->dest ) );
	rc = 0;
	goto done;

  found_conn:
	/* Stop the timer */
	stop_timer ( &conn->timer );

	/* Set the advertised window */
	conn->snd_win = tcphdr->win;

	/* TCP State Machine */
	conn->tcp_lstate = conn->tcp_state;
	switch ( conn->tcp_state ) {
	case TCP_CLOSED:
		DBG ( "tcp_rx(): Invalid state %s\n",
				tcp_states[conn->tcp_state] );
		rc = -EINVAL;
		goto done;
	case TCP_LISTEN:
		if ( tcphdr->flags & TCP_SYN ) {
			tcp_trans ( conn, TCP_SYN_RCVD );
			/* Synchronize the sequence numbers */
			conn->rcv_nxt = ntohl ( tcphdr->seq ) + 1;
			conn->tcp_flags |= TCP_ACK;

			/* Set the sequence number for the snd stream */
			conn->snd_una = random();
			conn->tcp_flags |= TCP_SYN;

			/* Send a SYN,ACK packet */
			goto send_tcp_nomsg;
		}
		/* Unexpected packet */
		goto unexpected;
	case TCP_SYN_SENT:
		if ( tcphdr->flags & TCP_SYN ) {
			/* Synchronize the sequence number in rcv stream */
			conn->rcv_nxt = ntohl ( tcphdr->seq ) + 1;
			conn->tcp_flags |= TCP_ACK;

			if ( tcphdr->flags & TCP_ACK ) {
				tcp_trans ( conn, TCP_ESTABLISHED );
				/**
				 * Process ACK of SYN. This does not invoke the
				 * acked() callback function.
				 */
				conn->snd_una = ntohl ( tcphdr->ack );
				if ( conn->tcp_op->connected )
					conn->tcp_op->connected ( conn );
				conn->tcp_flags |= TCP_ACK;
				tcp_senddata ( conn );
				rc = 0;
				goto done;
			} else {
				tcp_trans ( conn, TCP_SYN_RCVD );
				conn->tcp_flags |= TCP_SYN;
				goto send_tcp_nomsg;
			}
		}
		/* Unexpected packet */
		goto unexpected;
	case TCP_SYN_RCVD:
		if ( tcphdr->flags & TCP_RST ) {
			tcp_trans ( conn, TCP_LISTEN );
			if ( conn->tcp_op->closed )
				conn->tcp_op->closed ( conn, CONN_RESTART );
			rc = 0;
			goto done;
		}
		if ( tcphdr->flags & TCP_ACK ) {
			tcp_trans ( conn, TCP_ESTABLISHED );
			/**
			 * Process ACK of SYN. It neither invokes the callback
			 * function nor does it send an ACK.
			 */
			conn->snd_una = tcphdr->ack - 1;
			if ( conn->tcp_op->connected )
				conn->tcp_op->connected ( conn );
			rc = 0;
			goto done;
		}
		/* Unexpected packet */
		goto unexpected;
	case TCP_ESTABLISHED:
		if ( tcphdr->flags & TCP_FIN ) {
			if ( tcphdr->flags & TCP_ACK ) {
				tcp_trans ( conn, TCP_LAST_ACK );
				conn->tcp_flags |= TCP_FIN;
			} else {
				tcp_trans ( conn, TCP_CLOSE_WAIT );
			}
			/* FIN consumes one byte */
			conn->rcv_nxt++;
			conn->tcp_flags |= TCP_ACK;
			/* Send the packet */
			goto send_tcp_nomsg;
		}
		/* Packet might contain data */
		break;
	case TCP_FIN_WAIT_1:
		if ( tcphdr->flags & TCP_FIN ) {
			conn->rcv_nxt++;
			conn->tcp_flags |= TCP_ACK;
			if ( conn->tcp_op->closed )
				conn->tcp_op->closed ( conn, CONN_SNDCLOSE );

			if ( tcphdr->flags & TCP_ACK ) {
				tcp_trans ( conn, TCP_TIME_WAIT );
			} else {
				tcp_trans ( conn, TCP_CLOSING );
			}
			/* Send an acknowledgement */
			goto send_tcp_nomsg;
		}
		if ( tcphdr->flags & TCP_ACK ) {
			tcp_trans ( conn, TCP_FIN_WAIT_2 );
		}
		/* Packet might contain data */
		break;
	case TCP_FIN_WAIT_2:
		if ( tcphdr->flags & TCP_FIN ) {
			tcp_trans ( conn, TCP_TIME_WAIT );
			/* FIN consumes one byte */
			conn->rcv_nxt++;
			conn->tcp_flags |= TCP_ACK;
			goto send_tcp_nomsg;
		}
		/* Packet might contain data */
		break;
	case TCP_CLOSING:
		if ( tcphdr->flags & TCP_ACK ) {
			tcp_trans ( conn, TCP_TIME_WAIT );
			start_timer ( &conn->timer );
			rc = 0;
			goto done;
		}
		/* Unexpected packet */
		goto unexpected;
	case TCP_TIME_WAIT:
		/* Unexpected packet */
		goto unexpected;
	case TCP_CLOSE_WAIT:
		/* Packet could acknowledge data */
		break;
	case TCP_LAST_ACK:
		if ( tcphdr->flags & TCP_ACK ) {
			tcp_trans ( conn, TCP_CLOSED );
			rc = 0;
			goto done;
		}
		/* Unexpected packet */
		goto unexpected;
	}

	/**
	 * Any packet reaching this point either contains new data or
	 * acknowledges previously transmitted data.
	 */
	assert ( ( tcphdr->flags & TCP_ACK ) ||
		 pkb_len ( pkb ) > sizeof ( *tcphdr ) );

	/**
	 * Check if the received packet ACKs sent data
	 */
	if ( tcphdr->flags & TCP_ACK ) {
		acked = ntohl ( tcphdr->ack ) - conn->snd_una;
		if ( acked < 0 ) {
			/* Packet ACKs previously ACKed data */
			DBG ( "Previously ACKed data %lx\n", 
						ntohl ( tcphdr->ack ) );
			rc = 0;
			goto done;
		}
		/* Invoke the acked() callback */
		conn->snd_una += acked;
		if ( conn->tcp_op->acked )
			conn->tcp_op->acked ( conn, acked );
	}
	
	/**
	 * Check if packet contains new data
	 */
	toack = pkb_len ( pkb ) - hlen;
	if ( toack >= 0 ) {
		/* Check the sequence number */
		if ( conn->rcv_nxt == ntohl ( tcphdr->seq ) ) {
			conn->rcv_nxt += toack;
			if ( conn->tcp_op->newdata )
				conn->tcp_op->newdata ( conn, pkb->data + hlen,
							toack );
		} else {
			DBG ( "Unexpected sequence number %lx (wanted %lx)\n",
				ntohl ( tcphdr->ack ), conn->rcv_nxt );
		}
		conn->tcp_flags |= TCP_ACK;
	}
	
	/**
	 * Send data
	 */
	tcp_senddata ( conn );
	rc = 0;
	goto done;

  send_tcp_nomsg:
	free_pkb ( conn->tx_pkb );
	conn->tx_pkb = alloc_pkb ( MIN_PKB_LEN );
	pkb_reserve ( conn->tx_pkb, MAX_HDR_LEN );
	if ( ( rc = tcp_send ( conn, TCP_NOMSG, TCP_NOMSG_LEN ) ) != 0 ) {
		DBG ( "Error sending TCP message (rc = %d)\n", rc );
	}
	goto done;

  unexpected:
	DBG ( "Unexpected packet received in %s with flags = %#hx\n",
			tcp_states[conn->tcp_state], tcphdr->flags & TCP_MASK_FLAGS );
	tcp_close ( conn );
	free_pkb ( conn->tx_pkb );
	conn->tx_pkb = NULL;
	rc = -EINVAL;
	goto done;

 done:
	free_pkb ( pkb );
	return rc;
}

/** TCP protocol */
struct tcpip_protocol tcp_protocol __tcpip_protocol = {
	.name = "TCP",
	.rx = tcp_rx,
	.tcpip_proto = IP_TCP,
	.csum_offset = 16,
};

#endif /* USE_UIP */
