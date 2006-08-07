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
void tcp_trans ( struct tcp_connection *conn, int nxt_state ) {
	/* Remember the last state */
	conn->tcp_lstate = conn->tcp_state;
	conn->tcp_state = nxt_state;

	/* TODO: Check if this check is required */
	if ( conn->tcp_lstate == conn->tcp_state || 
	     conn->tcp_state == TCP_INVALID ) {
		conn->tcp_flags = 0;
		return;
	}

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
		break;
	default:
		DBG ( "TCP_INVALID state %d\n", conn->tcp_state );
		return;
	}
}

/**
 * Dump TCP header
 *
 * @v tcphdr	TCP header
 */
void tcp_dump ( struct tcp_header *tcphdr ) {
	DBG ( "TCP header at %p+%d\n", tcphdr, sizeof ( *tcphdr ) );
	DBG ( "\tSource port = %d, Destination port = %d\n",
		ntohs ( tcphdr->src ), ntohs ( tcphdr->dest ) );
	DBG ( "\tSequence Number = %ld, Acknowledgement Number = %ld\n",
		ntohl ( tcphdr->seq ), ntohl ( tcphdr->ack ) );
	DBG ( "\tHeader length (/4) = %hd, Flags [..RAPUSF]= %#x\n",
		( ( tcphdr->hlen & TCP_MASK_HLEN ) / 16 ),
		( tcphdr->flags & TCP_MASK_FLAGS ) );
	DBG ( "\tAdvertised window = %ld, Checksum = %x, Urgent Pointer = %d\n",
		ntohs ( tcphdr->win ), tcphdr->csum, ntohs ( tcphdr->urg ) );
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
	if ( over ) {
		conn = ( struct tcp_connection * ) container_of ( timer, 
						struct tcp_connection, timer );
		switch ( conn->tcp_state ) {
		case TCP_SYN_SENT:
			if ( conn->retransmits > MAX_RETRANSMITS ) {
				tcp_trans ( conn, TCP_CLOSED );
				return;
			}
			if ( conn->tcp_lstate == TCP_CLOSED ||
			     conn->tcp_lstate == TCP_LISTEN ) {
				goto send_tcp_nomsg;
			}
			return;
		case TCP_SYN_RCVD:
			tcp_trans ( conn, TCP_CLOSED );
			if ( conn->tcp_lstate == TCP_LISTEN ||
			     conn->tcp_lstate == TCP_SYN_SENT ) {
				goto send_tcp_nomsg;
			}
			return;
		case TCP_ESTABLISHED:
			break;
		case TCP_FIN_WAIT_1:
		case TCP_FIN_WAIT_2:
		case TCP_CLOSE_WAIT:
			goto send_tcp_nomsg;
		case TCP_CLOSING:
		case TCP_LAST_ACK:
			return;
		case TCP_TIME_WAIT:
			tcp_trans ( conn, TCP_CLOSED );
			return;
		}
		/* Retransmit the data */
		tcp_senddata ( conn );
		conn->retransmits++;
		return;

  send_tcp_nomsg:
		tcp_send ( conn, TCP_NOMSG, TCP_NOMSG_LEN );
		return;
	}
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
	conn->snd_una = ( ( ( uint32_t ) random() ) << 16 ) & random();
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
		conn->tcp_op->closed ( conn, CONN_SNDCLOSE );
		return 0;
	case TCP_CLOSE_WAIT:
		tcp_trans ( conn, TCP_LAST_ACK );
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
 * Listen for a packet
 *
 * @v conn	TCP connection
 * @v port	Local port, in network byte order
 *
 * This function adds the connection to a list of registered tcp connections. If
 * the local port is 0, the connection is assigned the lowest available port
 * between MIN_TCP_PORT and 65535.
 */
int tcp_listen ( struct tcp_connection *conn, uint16_t port ) {
	struct tcp_connection *cconn;
	if ( port != 0 ) {
		list_for_each_entry ( cconn, &tcp_conns, list ) {
			if ( cconn->local_port == port ) {
				DBG ( "Error listening to %d\n", 
							ntohs ( port ) );
				return -EISCONN;
			}
		}
		/* Add the connection to the list of registered connections */
		conn->local_port = port;
		list_add ( &conn->list, &tcp_conns );
		return 0;
	}
	/* Assigning lowest port not supported */
	DBG ( "Assigning lowest port not implemented\n");
	return -ENOSYS;
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
		conn->snd_una = ( ( ( uint32_t ) random() ) << 16 ) & random();
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
	conn->tcp_op->senddata ( conn, conn->tx_pkb->data, 
					pkb_available ( conn->tx_pkb ) );
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
	struct pk_buff *pkb = conn->tx_pkb;
	int slen;

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
	start_timer ( &conn->timer );

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

	/* Sanity check */
	if ( pkb_len ( pkb ) < sizeof ( *tcphdr ) ) {
		DBG ( "Packet too short (%d bytes)\n", pkb_len ( pkb ) );
		return -EINVAL;
	}

	/* Process TCP header */
	tcphdr = pkb->data;

	/* Verify header length */
	hlen = ( ( tcphdr->hlen & TCP_MASK_HLEN ) / 16 ) * 4;
	if ( hlen != sizeof ( *tcphdr ) ) {
		DBG ( "Bad header length (%d bytes)\n", hlen );
		return -EINVAL;
	}
	
	/* TODO: Verify checksum */
	
	/* Demux TCP connection */
	list_for_each_entry ( conn, &tcp_conns, list ) {
		if ( tcphdr->dest == conn->local_port ) {
			goto found_conn;
		}
	}
	
	DBG ( "No connection found on port %d\n", ntohs ( tcphdr->dest ) );
	return 0;

  found_conn:
	/* Stop the timer */
	stop_timer ( &conn->timer );
	conn->retransmits = 0;

	/* Set the advertised window */
	conn->snd_win = tcphdr->win;

	/* TCP State Machine */
	uint8_t out_flags = 0;
	conn->tcp_lstate = conn->tcp_state;
	switch ( conn->tcp_state ) {
	case TCP_CLOSED:
		DBG ( "tcp_rx(): Invalid state %s\n",
				tcp_states[conn->tcp_state] );
		return -EINVAL;
	case TCP_LISTEN:
		if ( tcphdr->flags & TCP_SYN ) {
			tcp_trans ( conn, TCP_SYN_RCVD );
			/* Synchronize the sequence numbers */
			conn->rcv_nxt = ntohl ( tcphdr->seq ) + 1;
			out_flags |= TCP_ACK;

			/* Set the sequence number for the snd stream */
			conn->snd_una = ( ( ( uint32_t ) random() ) << 16 );
			conn->snd_una &= random();
			out_flags |= TCP_SYN;

			/* Send a SYN,ACK packet */
			goto send_tcp_nomsg;
		}
		/* Unexpected packet */
		goto unexpected;
	case TCP_SYN_SENT:
		if ( tcphdr->flags & TCP_SYN ) {
			/* Synchronize the sequence number in rcv stream */
			conn->rcv_nxt = ntohl ( tcphdr->seq ) + 1;
			out_flags |= TCP_ACK;

			if ( tcphdr->flags & TCP_ACK ) {
				tcp_trans ( conn, TCP_ESTABLISHED );
				/**
				 * Process ACK of SYN. This does not invoke the
				 * acked() callback function.
				 */
				conn->snd_una = ntohl ( tcphdr->ack );
				conn->tcp_op->connected ( conn );
			} else {
				tcp_trans ( conn, TCP_SYN_RCVD );
				out_flags |= TCP_SYN;
			}
			/* Send SYN,ACK or ACK packet */
			goto send_tcp_nomsg;
		}
		/* Unexpected packet */
		goto unexpected;
	case TCP_SYN_RCVD:
		if ( tcphdr->flags & TCP_RST ) {
			tcp_trans ( conn, TCP_LISTEN );
			conn->tcp_op->closed ( conn, CONN_RESTART );
			return 0;
		}
		if ( tcphdr->flags & TCP_ACK ) {
			tcp_trans ( conn, TCP_ESTABLISHED );
			/**
			 * Process ACK of SYN. It neither invokes the callback
			 * function nor does it send an ACK.
			 */
			conn->snd_una = tcphdr->ack - 1;
			conn->tcp_op->connected ( conn );
			return 0;
		}
		/* Unexpected packet */
		goto unexpected;
	case TCP_ESTABLISHED:
		if ( tcphdr->flags & TCP_FIN ) {
			tcp_trans ( conn, TCP_CLOSE_WAIT );
			/* FIN consumes one byte */
			conn->rcv_nxt++;
			out_flags |= TCP_ACK;
			/* Send an acknowledgement */
			goto send_tcp_nomsg;
		}
		/* Packet might contain data */
		break;
	case TCP_FIN_WAIT_1:
		if ( tcphdr->flags & TCP_FIN ) {
			conn->rcv_nxt++;
			out_flags |= TCP_ACK;
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
			out_flags |= TCP_ACK;
			goto send_tcp_nomsg;
		}
		/* Packet might contain data */
		break;
	case TCP_CLOSING:
		if ( tcphdr->flags & TCP_ACK ) {
			tcp_trans ( conn, TCP_TIME_WAIT );
			return 0;
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
			return 0;
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

	/* Check for new data */
	toack = pkb_len ( pkb ) - hlen;
	if ( toack > 0 ) {
		/* Check if expected sequence number */
		if ( conn->rcv_nxt == ntohl ( tcphdr->seq ) ) {
			conn->rcv_nxt += toack;
			conn->tcp_op->newdata ( conn, pkb->data + sizeof ( *tcphdr ), toack );
		}

		/* Acknowledge new data */
		out_flags |= TCP_ACK;
		if ( !( tcphdr->flags & TCP_ACK ) ) {
			goto send_tcp_nomsg;
		}
	}

	/* Process ACK */
	if ( tcphdr->flags & TCP_ACK ) {
		acked = ntohl ( tcphdr->ack ) - conn->snd_una;
		if ( acked < 0 ) { /* TODO: Replace all uint32_t arith */
			DBG ( "Previously ACKed (%d)\n", tcphdr->ack );
			return 0;
		}
		/* Advance snd stream */
		conn->snd_una += acked;
		/* Set the ACK flag */
		conn->tcp_flags |= TCP_ACK;
		/* Invoke the acked() callback function */
		conn->tcp_op->acked ( conn, acked );
		/* Invoke the senddata() callback function */
		tcp_senddata ( conn );
	}
	return 0;

  send_tcp_nomsg:
	free_pkb ( conn->tx_pkb );
	conn->tx_pkb = alloc_pkb ( MIN_PKB_LEN );
	pkb_reserve ( conn->tx_pkb, MAX_HDR_LEN );
	int rc;
	if ( ( rc = tcp_send ( conn, TCP_NOMSG, TCP_NOMSG_LEN ) ) != 0 ) {
		DBG ( "Error sending TCP message (rc = %d)\n", rc );
	}
	return 0;

  unexpected:
	DBG ( "Unexpected packet received in %d state with flags = %hd\n",
			conn->tcp_state, tcphdr->flags & TCP_MASK_FLAGS );
	free_pkb ( conn->tx_pkb );
	return -EINVAL;
}

/** TCP protocol */
struct tcpip_protocol tcp_protocol = {
	.name = "TCP",
	.rx = tcp_rx,
	.tcpip_proto = IP_TCP,
	.csum_offset = 16,
};

TCPIP_PROTOCOL ( tcp_protocol );

#endif /* USE_UIP */
