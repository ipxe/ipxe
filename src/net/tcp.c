#include <string.h>
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
