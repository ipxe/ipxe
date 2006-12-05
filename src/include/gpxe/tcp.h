#ifndef _GPXE_TCP_H
#define _GPXE_TCP_H

/** @file
 *
 * TCP protocol
 *
 * This file defines the gPXE TCP API.
 *
 */

#include <stddef.h>
#include <gpxe/list.h>
#include <gpxe/tcpip.h>
#include <gpxe/pkbuff.h>
#include <gpxe/retry.h>

struct tcp_connection;

/**
 * TCP operations
 *
 */
struct tcp_operations {
	/*
	 * Connection closed
	 *
	 * @v conn	TCP connection
	 * @v status	Error code, if any
	 *
	 * This is called when the connection is closed for any
	 * reason, including timeouts or aborts.  The status code
	 * contains the negative error number, if the closure is due
	 * to an error.
	 *
	 * Note that acked() and newdata() may be called after
	 * closed(), if the packet containing the FIN also
	 * acknowledged data or contained new data.  Note also that
	 * connected() may not have been called before closed(), if
	 * the close is due to an error.
	 */
	void ( * closed ) ( struct tcp_connection *conn, int status );
	/**
	 * Connection established (SYNACK received)
	 *
	 * @v conn	TCP connection
	 */
	void ( * connected ) ( struct tcp_connection *conn );
	/**
	 * Data acknowledged
	 *
	 * @v conn	TCP connection
	 * @v len	Length of acknowledged data
	 *
	 * @c len is guaranteed to not exceed the outstanding amount
	 * of unacknowledged data.
	 */
	void ( * acked ) ( struct tcp_connection *conn, size_t len );
	/**
	 * New data received
	 *
	 * @v conn	TCP connection
	 * @v data	Data
	 * @v len	Length of data
	 */
	void ( * newdata ) ( struct tcp_connection *conn,
			     void *data, size_t len );
	/**
	 * Transmit data
	 *
	 * @v conn	TCP connection
	 * @v buf	Temporary data buffer
	 * @v len	Length of temporary data buffer
	 *
	 * The application should transmit whatever it currently wants
	 * to send using tcp_send().  If retransmissions are required,
	 * senddata() will be called again and the application must
	 * regenerate the data.  The easiest way to implement this is
	 * to ensure that senddata() never changes the application's
	 * state.
	 *
	 * The application may use the temporary data buffer to
	 * construct the data to be sent.  Note that merely filling
	 * the buffer will do nothing; the application must call
	 * tcp_send() in order to actually transmit the data.  Use of
	 * the buffer is not compulsory; the application may call
	 * tcp_send() on any block of data.
	 */
	void ( * senddata ) ( struct tcp_connection *conn, void *buf,
			      size_t len );
};

#if USE_UIP

/**
 * A TCP connection
 *
 */
struct tcp_connection {
	/** Address of the remote end of the connection */
	struct sockaddr_in sin;
	/** Operations table for this connection */
	struct tcp_operations *tcp_op;
};

extern void tcp_connect ( struct tcp_connection *conn );
extern void tcp_send ( struct tcp_connection *conn, const void *data,
		       size_t len );
extern void tcp_kick ( struct tcp_connection *conn );
extern void tcp_close ( struct tcp_connection *conn );

#else

#define TCP_NOMSG ""
#define TCP_NOMSG_LEN 0

/* Smallest port number on which a TCP connection can listen */
#define TCP_MIN_PORT 1

/* Some PKB constants */
#define MAX_HDR_LEN	100
#define MAX_PKB_LEN	1500
#define MIN_PKB_LEN	MAX_HDR_LEN + 100 /* To account for padding by LL */

/**
 * TCP states
 */
#define TCP_CLOSED	0
#define TCP_LISTEN	1
#define TCP_SYN_SENT	2
#define TCP_SYN_RCVD	3
#define TCP_ESTABLISHED	4
#define TCP_FIN_WAIT_1	5
#define TCP_FIN_WAIT_2	6
#define TCP_CLOSING	7
#define TCP_TIME_WAIT	8
#define TCP_CLOSE_WAIT	9
#define TCP_LAST_ACK	10

#define TCP_INVALID	11

/**
 * A TCP connection
 */
struct tcp_connection {
	struct sockaddr_tcpip peer;	/* Remote socket address */
	uint16_t local_port;		/* Local port, in network byte order */
	int tcp_state;			/* TCP state */
	int tcp_lstate;			/* Last TCP state */
	uint32_t snd_una;		/* Lowest unacked byte on snd stream */
	uint32_t snd_win;		/* Offered by remote end */
	uint32_t rcv_nxt;		/* Next expected byte on rcv stream */
	uint32_t rcv_win;		/* Advertised to receiver */
	uint8_t tcp_flags;		/* TCP header flags */
	struct list_head list;		/* List of TCP connections */
	struct pk_buff *tx_pkb;		/* Transmit packet buffer */
	struct retry_timer timer;	/* Retransmission timer */
	struct tcp_operations *tcp_op;	/* Operations table for connection */
};

/** Retry timer values */
#define MAX_RETRANSMITS	3

/**
 * Connection closed status codes
 */
#define CONN_SNDCLOSE	0
#define CONN_RESTART	1
#define CONN_TIMEOUT	2
#define CONN_RCVCLOSE	3

/**
 * A TCP header
 */
struct tcp_header {
	uint16_t src;		/* Source port */
	uint16_t dest;		/* Destination port */
	uint32_t seq;		/* Sequence number */
	uint32_t ack;		/* Acknowledgement number */
	uint8_t hlen;		/* Header length (4), Reserved (4) */
	uint8_t flags;		/* Reserved (2), Flags (6) */
	uint16_t win;		/* Advertised window */
	uint16_t csum;		/* Checksum */
	uint16_t urg;		/* Urgent pointer */
};

/**
 * TCP masks
 */
#define TCP_MASK_HLEN	0xf0
#define TCP_MASK_FLAGS	0x3f

/**
 * TCP flags
 */
#define TCP_URG		0x20
#define TCP_ACK		0x10
#define TCP_PSH		0x08
#define TCP_RST		0x04
#define TCP_SYN		0x02
#define TCP_FIN		0x01

extern struct tcpip_protocol tcp_protocol;

static inline int tcp_closed ( struct tcp_connection *conn ) {
	return ( conn->tcp_state == TCP_CLOSED );
}

extern void tcp_init_conn ( struct tcp_connection *conn );
extern int tcp_connect ( struct tcp_connection *conn );
extern int tcp_connectto ( struct tcp_connection *conn,
			   struct sockaddr_tcpip *peer );
extern int tcp_listen ( struct tcp_connection *conn, uint16_t port );
extern int tcp_senddata ( struct tcp_connection *conn );
extern int tcp_close ( struct tcp_connection *conn );

extern int tcp_send ( struct tcp_connection *conn, const void *data, 
		      size_t len );

#endif /* USE_UIP */

#endif /* _GPXE_TCP_H */
