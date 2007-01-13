#ifndef _GPXE_TCP_H
#define _GPXE_TCP_H

/** @file
 *
 * TCP protocol
 *
 * This file defines the gPXE TCP API.
 *
 */

#include "latch.h"
#include <gpxe/tcpip.h>

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
 * TCP MSS option
 */
struct tcp_mss_option {
	uint8_t kind;
	uint8_t length;
	uint16_t mss;
};

/** Code for the TCP MSS option */
#define TCP_OPTION_MSS 2

/*
 * TCP flags
 */
#define TCP_CWR		0x80
#define TCP_ECE		0x40
#define TCP_URG		0x20
#define TCP_ACK		0x10
#define TCP_PSH		0x08
#define TCP_RST		0x04
#define TCP_SYN		0x02
#define TCP_FIN		0x01

/**
* @defgroup tcpstates TCP states
*
* The TCP state is defined by a combination of the flags that have
* been sent to the peer, the flags that have been acknowledged by the
* peer, and the flags that have been received from the peer.
*
* @{
*/

/** TCP flags that have been sent in outgoing packets */
#define TCP_STATE_SENT(flags) ( (flags) << 0 )
#define TCP_FLAGS_SENT(state) ( ( (state) >> 0 ) & 0xff )

/** TCP flags that have been acknowledged by the peer
 *
 * Note that this applies only to SYN and FIN.
 */
#define TCP_STATE_ACKED(flags) ( (flags) << 8 )
#define TCP_FLAGS_ACKED(state) ( ( (state) >> 8 ) & 0xff )

/** TCP flags that have been received from the peer
 *
 * Note that this applies only to SYN and FIN, and that once SYN has
 * been received, we should always be sending ACK.
 */
#define TCP_STATE_RCVD(flags) ( (flags) << 16 )
#define TCP_FLAGS_RCVD(state) ( ( (state) >> 16 ) & 0xff )

/** TCP flags that are currently being sent in outgoing packets */
#define TCP_FLAGS_SENDING(state) \
	( TCP_FLAGS_SENT ( state ) & ~TCP_FLAGS_ACKED ( state ) )

/** CLOSED
 *
 * The connection has not yet been used for anything.
 */
#define TCP_CLOSED TCP_RST

/** LISTEN
 *
 * Not currently used as a state; we have no support for listening
 * connections.  Given a unique value to avoid compiler warnings.
 */
#define TCP_LISTEN 0

/** SYN_SENT
 *
 * SYN has been sent, nothing has yet been received or acknowledged.
 */
#define TCP_SYN_SENT	( TCP_STATE_SENT ( TCP_SYN ) )

/** SYN_RCVD
 *
 * SYN has been sent but not acknowledged, SYN has been received.
 */
#define TCP_SYN_RCVD	( TCP_STATE_SENT ( TCP_SYN | TCP_ACK ) |	    \
			  TCP_STATE_RCVD ( TCP_SYN ) )

/** ESTABLISHED
 *
 * SYN has been sent and acknowledged, SYN has been received.
 */
#define TCP_ESTABLISHED	( TCP_STATE_SENT ( TCP_SYN | TCP_ACK ) |	    \
			  TCP_STATE_ACKED ( TCP_SYN ) |			    \
			  TCP_STATE_RCVD ( TCP_SYN ) )

/** FIN_WAIT_1
 *
 * SYN has been sent and acknowledged, SYN has been received, FIN has
 * been sent but not acknowledged, FIN has not been received.
 *
 * RFC 793 shows that we can enter FIN_WAIT_1 without have had SYN
 * acknowledged, i.e. if the application closes the connection after
 * sending and receiving SYN, but before having had SYN acknowledged.
 * However, we have to *pretend* that SYN has been acknowledged
 * anyway, otherwise we end up sending SYN and FIN in the same
 * sequence number slot.  Therefore, when we transition from SYN_RCVD
 * to FIN_WAIT_1, we have to remember to set TCP_STATE_ACKED(TCP_SYN)
 * and increment our sequence number.
 */
#define TCP_FIN_WAIT_1	( TCP_STATE_SENT ( TCP_SYN | TCP_ACK | TCP_FIN ) |  \
			  TCP_STATE_ACKED ( TCP_SYN ) |			    \
			  TCP_STATE_RCVD ( TCP_SYN ) )

/** FIN_WAIT_2
 *
 * SYN has been sent and acknowledged, SYN has been received, FIN has
 * been sent and acknowledged, FIN ha not been received.
 */
#define TCP_FIN_WAIT_2	( TCP_STATE_SENT ( TCP_SYN | TCP_ACK | TCP_FIN ) |  \
			  TCP_STATE_ACKED ( TCP_SYN | TCP_FIN ) |	    \
			  TCP_STATE_RCVD ( TCP_SYN ) )

/** CLOSING / LAST_ACK
 *
 * SYN has been sent and acknowledged, SYN has been received, FIN has
 * been sent but not acknowledged, FIN has been received.
 *
 * This state actually encompasses both CLOSING and LAST_ACK; they are
 * identical with the definition of state that we use.  I don't
 * *believe* that they need to be distinguished.
 */
#define TCP_CLOSING_OR_LAST_ACK						    \
			( TCP_STATE_SENT ( TCP_SYN | TCP_ACK | TCP_FIN ) |  \
			  TCP_STATE_ACKED ( TCP_SYN ) |			    \
			  TCP_STATE_RCVD ( TCP_SYN | TCP_FIN ) )

/** TIME_WAIT
 *
 * SYN has been sent and acknowledged, SYN has been received, FIN has
 * been sent and acknowledged, FIN has been received.
 */
#define TCP_TIME_WAIT	( TCP_STATE_SENT ( TCP_SYN | TCP_ACK | TCP_FIN ) |  \
			  TCP_STATE_ACKED ( TCP_SYN | TCP_FIN ) |	    \
			  TCP_STATE_RCVD ( TCP_SYN | TCP_FIN ) )

/** CLOSE_WAIT
 *
 * SYN has been sent and acknowledged, SYN has been received, FIN has
 * been received.
 */
#define TCP_CLOSE_WAIT	( TCP_STATE_SENT ( TCP_SYN | TCP_ACK ) |	    \
			  TCP_STATE_ACKED ( TCP_SYN ) |			    \
			  TCP_STATE_RCVD ( TCP_SYN | TCP_FIN ) )

/** Can send data in current state
 *
 * We can send data if and only if we have had our SYN acked and we
 * have not yet sent our FIN.
 */
#define TCP_CAN_SEND_DATA(state)					    \
	( ( (state) & ( TCP_STATE_ACKED ( TCP_SYN ) |			    \
			TCP_STATE_SENT ( TCP_FIN ) ) )			    \
	  == TCP_STATE_ACKED ( TCP_SYN ) )

/** Have closed gracefully
 *
 * We have closed gracefully if we have both received a FIN and had
 * our own FIN acked.
 */
#define TCP_CLOSED_GRACEFULLY(state)					    \
	( ( (state) & ( TCP_STATE_ACKED ( TCP_FIN ) |			    \
			TCP_STATE_RCVD ( TCP_FIN ) ) )			    \
	  == ( TCP_STATE_ACKED ( TCP_FIN ) | TCP_STATE_RCVD ( TCP_FIN ) ) )

/** @} */

/** Mask for TCP header length field */
#define TCP_MASK_HLEN	0xf0

/** Smallest port number on which a TCP connection can listen */
#define TCP_MIN_PORT 1

/* Some PKB constants */
#define MAX_HDR_LEN	100
#define MAX_PKB_LEN	1500
#define MIN_PKB_LEN	MAX_HDR_LEN + 100 /* To account for padding by LL */

/**
 * Advertised TCP window size
 *
 * Our TCP window is actually limited by the amount of space available
 * for RX packets in the NIC's RX ring; we tend to populate the rings
 * with far fewer descriptors than a typical driver.  Since we have no
 * way of knowing how much of this RX ring space will be available for
 * received TCP packets (consider, for example, that they may all be
 * consumed by a series of unrelated ARP requests between other
 * machines on the network), it is actually not even theoretically
 * possible for us to specify an accurate window size.  We therefore
 * guess an arbitrary number that is empirically as large as possible
 * while avoiding retransmissions due to dropped packets.
 */
#define TCP_WINDOW_SIZE	4096

/**
 * Advertised TCP MSS
 *
 * We currently hardcode this to a reasonable value and hope that the
 * sender uses path MTU discovery.  The alternative is breaking the
 * abstraction layer so that we can find out the MTU from the IP layer
 * (which would have to find out from the net device layer).
 */
#define TCP_MSS 1460

/** TCP maximum segment lifetime
 *
 * Currently set to 2 minutes, as per RFC 793.
 */
#define TCP_MSL ( 2 * 60 * TICKS_PER_SEC )

struct tcp_application;

/**
 * TCP operations
 *
 */
struct tcp_operations {
	/*
	 * Connection closed
	 *
	 * @v app	TCP application
	 * @v status	Error code, if any
	 *
	 * This is called when the connection is closed for any
	 * reason, including timeouts or aborts.  The status code
	 * contains the negative error number, if the closure is due
	 * to an error.
	 *
	 * When closed() is called, the application no longer has a
	 * valid TCP connection.  Note that connected() may not have
	 * been called before closed(), if the close is due to an
	 * error during connection setup.
	 */
	void ( * closed ) ( struct tcp_application *app, int status );
	/**
	 * Connection established
	 *
	 * @v app	TCP application
	 */
	void ( * connected ) ( struct tcp_application *app );
	/**
	 * Data acknowledged
	 *
	 * @v app	TCP application
	 * @v len	Length of acknowledged data
	 *
	 * @c len is guaranteed to not exceed the outstanding amount
	 * of unacknowledged data.
	 */
	void ( * acked ) ( struct tcp_application *app, size_t len );
	/**
	 * New data received
	 *
	 * @v app	TCP application
	 * @v data	Data
	 * @v len	Length of data
	 */
	void ( * newdata ) ( struct tcp_application *app,
			     void *data, size_t len );
	/**
	 * Transmit data
	 *
	 * @v app	TCP application
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
	void ( * senddata ) ( struct tcp_application *app, void *buf,
			      size_t len );
};

struct tcp_connection;

/**
 * A TCP application
 *
 * This data structure represents an application with a TCP connection.
 */
struct tcp_application {
	/** TCP connection data
	 *
	 * This is filled in by TCP calls that initiate a connection,
	 * and reset to NULL when the connection is closed.
	 */
	struct tcp_connection *conn;
	/** TCP connection operations table */
	struct tcp_operations *tcp_op;
};

extern int tcp_connect ( struct tcp_application *app,
			 struct sockaddr_tcpip *peer,
			 uint16_t local_port );
extern void tcp_close ( struct tcp_application *app );
extern int tcp_senddata ( struct tcp_application *app );
extern int tcp_send ( struct tcp_application *app, const void *data, 
		      size_t len );

extern struct tcpip_protocol tcp_protocol;

#endif /* _GPXE_TCP_H */
