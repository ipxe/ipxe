#ifndef _IPXE_NBD_H
#define _IPXE_NBD_H

/** @file
 *
 * Network Block Device (NBD) protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/refcnt.h>

/** Default NBD port */
#define DEFAULT_NBD_PORT 10809

/* Magic values */

/* 'NBDMAGIC' */
#define NBD_INIT_PASSWD         0x4e42444d41474943ULL 
/* 'IHAVEOPT' */
#define NBD_OPT_REQ_MAGIC       0x49484156454f5054ULL
#define NBD_OPT_REPLY_MAGIC     0x3e889045565a9ULL

#define NBD_REQUEST_MAGIC       0x25609513
#define NBD_REPLY_MAGIC         0x67446698

#define NBD_ZERO_PAD_LEN        124

/* Handshake flags */
#define NBD_FLAG_FIXED_NEWSTYLE (1 << 0)
#define NBD_FLAG_NO_ZEROES      (1 << 1)

/* Flags from client to server */
#define NBD_FLAG_C_FIXED_NEWSTYLE NBD_FLAG_FIXED_NEWSTYLE
#define NBD_FLAG_C_NO_ZEROES    NBD_FLAG_NO_ZEROES

/* Options that the client can select to the server */
#define NBD_OPT_EXPORT_NAME     (1)
#define NBD_OPT_GO              (7)

/* Info types */
#define NBD_INFO_EXPORT         (0)

/* Transmission flags */
#define NBD_FLAG_HAS_FLAGS      (1 << 0)
#define NBD_FLAG_READ_ONLY      (1 << 1)

/* Replies the server can send during negotiation */
#define NBD_REP_ACK             (1)
#define NBD_REP_INFO            (3)
#define NBD_REP_FLAG_ERROR      (1U << 31)
#define NBD_REP_ERR_UNSUP       (1 | NBD_REP_FLAG_ERROR)
#define NBD_REP_ERR_UNKNOWN     (6 | NBD_REP_FLAG_ERROR)

/* Request types */
#define NBD_CMD_READ            (0)
#define NBD_CMD_WRITE           (1)


/* Initial negotiation (message from a server) */
struct nbd_proto_neg_init {
	uint64_t init_magic;
	uint64_t opt_magic;
	uint16_t handshake_flags;
} __attribute__ (( packed ));

/* Export name reply */
struct nbd_proto_export_name_reply {
	uint64_t export_size;
	uint16_t trans_flags;
	/* 124 bytes to discard */
} __attribute__ (( packed ));

/* Option request */
struct nbd_proto_opt_request {
	uint64_t request_magic;
	uint32_t option;
	uint32_t length;
	uint8_t data [0];
} __attribute__ (( packed ));

/* Option reply */
struct nbd_proto_opt_reply {
	uint64_t reply_magic;
	uint32_t option;
	uint32_t type;
	uint32_t length;
	uint8_t data [0];
} __attribute__ (( packed ));

/* The NBD_OPT_EXPORT_NAME option request */
struct nbd_proto_opt_export_name {
	struct nbd_proto_opt_request request;
	char export_name [0];
} __attribute__ (( packed ));

/* The NBD_OPT_GO option request */
struct nbd_proto_opt_go {
	struct nbd_proto_opt_request request;
	uint32_t name_length;
	char export_name [0];
} __attribute__ (( packed ));

/* Reply for NBD_INFO_EXPORT option */
struct nbd_proto_rep_info_export {
	uint16_t type; /* NBD_INFO_EXPORT */
	uint64_t export_size;
	uint16_t trans_flags;
} __attribute__ (( packed ));


/* Transmission request */
struct nbd_proto_trans_request {
	uint32_t request_magic;
	uint16_t flags;
	uint16_t type;
	uint64_t handle;
	uint64_t offset;
	uint32_t length;
} __attribute__ (( packed ));

/* Transmission (simple) reply */
struct nbd_proto_trans_reply {
	uint32_t reply_magic;
	uint32_t error;
	uint64_t handle;
} __attribute__ (( packed ));


/** State of a NBD RX engine */
enum nbd_rx_state {
	NBD_RX_NEG_INIT = 0,
	NBD_RX_NEG_EXP_NAME,
	NBD_RX_NEG_OPT_INFO,
	NBD_RX_NEG_REP_INFO,
	NBD_RX_TRANS_REP_CMD,
	NBD_RX_TRANS_DATA,
};

/** State of a NBD TX engine */
enum nbd_tx_state {
	NBD_TX_IDLE = 0,
	NBD_TX_NEG_OPT,
	NBD_TX_CMD_BRC,
	NBD_TX_CMD_HEADER,
	NBD_TX_CMD_DATA,
};

/** A NBD session */
struct nbd_session {
	/** Reference counter */
	struct refcnt refcnt;

	/** Block device interface */
	struct interface block;
	/** Transport layer interface */
	struct interface socket;

	/** NBD URI */
	struct uri *uri;

	/** Export name */
	const char *export_name;

	/** Use NBD_OPT_GO option for negotiation */
	int use_opt_go;

	/** State of a NBD RX engine */
	enum nbd_rx_state rx_state;

	/** Length of data to discard (e.g. unwanted zeroes or options */
	size_t discard_len;

	/* Expected information length of option reply */
	size_t reply_info_length;

	/** Byte offset within the receive buffer */
	size_t rx_offset;

	/** Buffer for received data, enough to hold reequest header */
	union {
		char rx_buffer [0];
		struct nbd_proto_neg_init rx_neg_init;
		struct nbd_proto_export_name_reply rx_exp_name_reply;
		struct nbd_proto_opt_reply rx_opt_reply;
		struct nbd_proto_rep_info_export rx_rep_info_export;
		struct nbd_proto_trans_reply rx_trans_reply;
	};

	/** Handshake flags, sent by a server */
	uint16_t handshake_flags;

	/* Transmission flags, sent using NBD_INFO_EXPORT */
	uint16_t trans_flags;

	/** Export size, sent using NBD_INFO_EXPORT */
	uint64_t export_size;

	/** State of a NBD TX engine */
	enum nbd_tx_state tx_state;

	/** TX process */
	struct process process;

	/** Command in progress */
	struct nbd_command *command;
};

#endif /* _IPXE_NBD_H */
