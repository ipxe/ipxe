#ifndef	_GPXE_TFTP_H
#define	_GPXE_TFTP_H

/** @file
 *
 * TFTP protocol
 *
 */

#include <stdint.h>
#include <gpxe/udp.h>
#include <gpxe/async.h>
#include <gpxe/retry.h>

#define TFTP_PORT	       69 /**< Default TFTP server port */
#define	TFTP_DEFAULT_BLKSIZE  512 /**< Default TFTP data block size */
#define	TFTP_MAX_BLKSIZE     1432

#define TFTP_RRQ		1 /**< Read request opcode */
#define TFTP_WRQ		2 /**< Write request opcode */
#define TFTP_DATA		3 /**< Data block opcode */
#define TFTP_ACK		4 /**< Data block acknowledgement opcode */
#define TFTP_ERROR		5 /**< Error opcode */
#define TFTP_OACK		6 /**< Options acknowledgement opcode */

#define TFTP_ERR_FILE_NOT_FOUND	1 /**< File not found */
#define TFTP_ERR_ACCESS_DENIED	2 /**< Access violation */
#define TFTP_ERR_DISK_FULL	3 /**< Disk full or allocation exceeded */
#define TFTP_ERR_ILLEGAL_OP	4 /**< Illegal TFTP operation */
#define TFTP_ERR_UNKNOWN_TID	5 /**< Unknown transfer ID */
#define TFTP_ERR_FILE_EXISTS	6 /**< File already exists */
#define TFTP_ERR_UNKNOWN_USER	7 /**< No such user */
#define TFTP_ERR_BAD_OPTS	8 /**< Option negotiation failed */

/** A TFTP read request (RRQ) packet */
struct tftp_rrq {
	uint16_t opcode;
	char data[0];
} __attribute__ (( packed ));

/** A TFTP data (DATA) packet */
struct tftp_data {
	uint16_t opcode;
	uint16_t block;
	uint8_t data[0];
} __attribute__ (( packed ));
 
/** A TFTP acknowledgement (ACK) packet */
struct tftp_ack {
	uint16_t opcode;
	uint16_t block;
} __attribute__ (( packed ));

/** A TFTP error (ERROR) packet */
struct tftp_error {
	uint16_t opcode;
	uint16_t errcode;
	char errmsg[0];
} __attribute__ (( packed ));

/** A TFTP options acknowledgement (OACK) packet */
struct tftp_oack {
	uint16_t opcode;
	char data[0];
} __attribute__ (( packed ));

/** The common header of all TFTP packets */
struct tftp_common {
	uint16_t opcode;
} __attribute__ (( packed ));

/** A union encapsulating all TFTP packet types */
union tftp_any {
	struct tftp_common	common;
	struct tftp_rrq		rrq;
	struct tftp_data	data;
	struct tftp_ack		ack;
	struct tftp_error	error;
	struct tftp_oack	oack;
};	

/**
 * A TFTP session
 *
 * This data structure holds the state for an ongoing TFTP transfer.
 */
struct tftp_session {
	/** UDP connection */
	struct udp_connection udp;
	/** Filename */
	const char *filename;

	/**
	 * Callback function
	 *
	 * @v tftp		TFTP connection
	 * @v block		Block number
	 * @v data		Data
	 * @v len		Length of data
	 */
	void ( * callback ) ( struct tftp_session *tftp, unsigned int block,
			      void *data, size_t len );
	/**
	 * Transfer ID
	 *
	 * This is the transfer ID allocated by the server, used as
	 * the server UDP port for all packets except the initial read
	 * request.
	 */
	uint16_t tid;
	/** Session state
	 *
	 * This is the block number to be used in the next ACK sent
	 * back to the server, i.e. the number of the last received
	 * data block.  The value zero indicates that the last
	 * received block was an OACK (i.e. that the next ACK will
	 * contain a block number of zero), and any value less than
	 * zero indicates that the connection has not yet been opened
	 * (i.e. that no blocks have yet been received).
	 */
	int state;
	/** Data block size
	 *
	 * This is the "blksize" option negotiated with the TFTP
	 * server.  (If the TFTP server does not support TFTP options,
	 * this will default to 512).
	 */
	unsigned int blksize;
	/** File size
	 *
	 * This is the value returned in the "tsize" option from the
	 * TFTP server.  If the TFTP server does not support the
	 * "tsize" option, this value will be zero.
	 */
	unsigned long tsize;
	
	/** Asynchronous operation for this session */
	struct async_operation aop;
	/** Retransmission timer */
	struct retry_timer timer;
};

#endif /* _GPXE_TFTP_H */
