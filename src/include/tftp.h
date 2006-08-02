#ifndef	TFTP_H
#define	TFTP_H

/** @file */

#include <gpxe/in.h>
#include "buffer.h"
#include "nic.h"
#include "ip.h"
#include "udp.h"

#define TFTP_PORT	69		/**< Default TFTP server port */
#define	TFTP_DEFAULT_BLKSIZE	512
#define	TFTP_MAX_BLKSIZE		1432 /* 512 */

#define TFTP_RRQ	1
#define TFTP_WRQ	2
#define TFTP_DATA	3
#define TFTP_ACK	4
#define TFTP_ERROR	5
#define TFTP_OACK	6

#define TFTP_ERR_FILE_NOT_FOUND	1 /**< File not found */
#define TFTP_ERR_ACCESS_DENIED	2 /**< Access violation */
#define TFTP_ERR_DISK_FULL	3 /**< Disk full or allocation exceeded */
#define TFTP_ERR_ILLEGAL_OP	4 /**< Illegal TFTP operation */
#define TFTP_ERR_UNKNOWN_TID	5 /**< Unknown transfer ID */
#define TFTP_ERR_FILE_EXISTS	6 /**< File already exists */
#define TFTP_ERR_UNKNOWN_USER	7 /**< No such user */
#define TFTP_ERR_BAD_OPTS	8 /**< Option negotiation failed */

/** A TFTP request (RRQ) packet */
struct tftp_rrq {
	struct iphdr ip;
	struct udphdr udp;
	uint16_t opcode;
	char data[TFTP_DEFAULT_BLKSIZE];
} PACKED;

/** A TFTP data (DATA) packet */
struct tftp_data {
	struct iphdr ip;
	struct udphdr udp;
	uint16_t opcode;
	uint16_t block;
	uint8_t data[TFTP_MAX_BLKSIZE];
} PACKED;
 
/** A TFTP acknowledgement (ACK) packet */
struct tftp_ack {
	struct iphdr ip;
	struct udphdr udp;
	uint16_t opcode;
	uint16_t block;
} PACKED;

/** A TFTP error (ERROR) packet */
struct tftp_error {
	struct iphdr ip;
	struct udphdr udp;
	uint16_t opcode;
	uint16_t errcode;
	char errmsg[TFTP_DEFAULT_BLKSIZE];
} PACKED;

/** A TFTP options acknowledgement (OACK) packet */
struct tftp_oack {
	struct iphdr ip;
	struct udphdr udp;
	uint16_t opcode;
	uint8_t data[TFTP_DEFAULT_BLKSIZE];
} PACKED;

/** The common header of all TFTP packets */
struct tftp_common {
	struct iphdr ip;
	struct udphdr udp;
	uint16_t opcode;
} PACKED;

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
 * TFTP state
 *
 * This data structure holds the state for an ongoing TFTP transfer.
 */
struct tftp_state {
	/** TFTP server address
	 *
	 * This is the IP address and UDP port from which data packets
	 * will be sent, and to which ACK packets should be sent.
	 */
	struct sockaddr_in server;
	/** TFTP client port
	 *
	 * This is the UDP port from which the open request will be
	 * sent, and to which any unicast data packets will be sent.
	 */
	uint16_t lport;
	/** TFTP multicast address
	 *
	 * This is the IP address and UDP port to which multicast data
	 * packets, if any, will be sent.
	 */
	struct sockaddr_in multicast;
	/** Master client
	 *
	 * This will be true if the client is the master client for a
	 * multicast protocol (i.e. MTFTP or TFTM).  (It will always
	 * be true for a non-multicast protocol, i.e. plain old TFTP).
	 */
	int master;
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
	off_t tsize;
	/** Last received block
	 *
	 * The block number of the most recent block received from the
	 * TFTP server.  Note that the first data block is block 1; a
	 * value of 0 indicates that no data blocks have yet been
	 * received.
	 *
	 * For multicast TFTP protocols, where the blocks may not be
	 * received in strict order, the meaning of this field changes
	 * slightly, to "first missing block minus one".  For example,
	 * suppose that we have received blocks 1, 2, 4 and 5; this
	 * field would then have the value 2, since the first missing
	 * block is block 3.  If the blocks do arrive in strict order,
	 * this definition is exactly equivalent to "most recently
	 * received block".
	 */
	unsigned int block;
};



struct tftpreq_info_t {
	struct sockaddr_in *server;
	const char *name;
	unsigned short blksize;
} PACKED;

struct tftpblk_info_t {
	char *data;
	unsigned int block;
	unsigned int len;
	int eof;
} PACKED;

#define TFTP_MIN_PACKET	(sizeof(struct iphdr) + sizeof(struct udphdr) + 4)

#endif	/* TFTP_H */
