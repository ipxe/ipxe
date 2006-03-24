#include <string.h>
#include <assert.h>
#include <byteswap.h>
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
 * Open a TCP connection
 *
 * @v conn	TCP connection
 * @ret 0	Success
 * @ret <0	Failure
 * 
 * This sets up a new TCP connection to the remote host specified in
 * tcp_connection::sin.  The actual SYN packet will not be sent out
 * until run_tcpip() is called for the first time.
 *
 * @todo Use linked lists instead of a static buffer, and thereby
 *       remove the only potential failure case, giving this function
 *       a void return type.
 */
int tcp_connect ( struct tcp_connection *conn ) {
	struct uip_conn *uip_conn;
	u16_t ipaddr[2];

	assert ( conn->sin.sin_addr.s_addr != 0 );
	assert ( conn->sin.sin_port != 0 );
	assert ( conn->tcp_op != NULL );
	assert ( sizeof ( uip_conn->appstate ) == sizeof ( conn ) );

	* ( ( uint32_t * ) ipaddr ) = conn->sin.sin_addr.s_addr;
	uip_conn = uip_connect ( ipaddr, conn->sin.sin_port );
	if ( ! uip_conn )
		return -1;

	*( ( void ** ) uip_conn->appstate ) = conn;
	return 0;
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
 * tcp_operations::newdata() method will be called again in order to
 * regenerate the data.
 */
void tcp_send ( struct tcp_connection *conn __unused,
		const void *data, size_t len ) {
	assert ( conn = *( ( void ** ) uip_conn->appstate ) );
	uip_send ( ( void * ) data, len );
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

	assert ( conn->tcp_op->closed != NULL );
	assert ( conn->tcp_op->connected != NULL );
	assert ( conn->tcp_op->acked != NULL );
	assert ( conn->tcp_op->newdata != NULL );
	assert ( conn->tcp_op->senddata != NULL );

	if ( uip_aborted() && op->aborted ) /* optional method */
		op->aborted ( conn );
	if ( uip_timedout() && op->timedout ) /* optional method */
		op->timedout ( conn );
	if ( uip_closed() && op->closed ) /* optional method */
		op->closed ( conn );
	if ( uip_connected() )
		op->connected ( conn );
	if ( uip_acked() )
		op->acked ( conn, uip_conn->len );
	if ( uip_newdata() )
		op->newdata ( conn, ( void * ) uip_appdata, uip_len );
	if ( uip_rexmit() || uip_newdata() || uip_acked() ||
	     uip_connected() || uip_poll() )
		op->senddata ( conn );
}

/* Present here to allow everything to link.  Will go into separate
 * udp.c file
 */
void uip_udp_appcall ( void ) {
}
