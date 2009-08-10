#ifndef _GPXE_SRP_H
#define _GPXE_SRP_H

/** @file
 *
 * SCSI RDMA Protocol
 *
 */

FILE_LICENCE ( BSD2 );

#include <stdint.h>
#include <byteswap.h>
#include <gpxe/iobuf.h>
#include <gpxe/xfer.h>
#include <gpxe/scsi.h>

/*****************************************************************************
 *
 * Common fields
 *
 *****************************************************************************
 */

/** An SRP information unit tag */
struct srp_tag {
	uint32_t dwords[2];
} __attribute__ (( packed ));

/** An SRP port ID */
struct srp_port_id {
	uint8_t bytes[16];
} __attribute__ (( packed ));

/** An SRP port ID pair */
struct srp_port_ids {
	/** Initiator port ID */
	struct srp_port_id initiator;
	/** Target port ID */
	struct srp_port_id target;
} __attribute__ (( packed ));

/** SRP information unit common fields */
struct srp_common {
	/** Information unit type */
	uint8_t type;
	/** Reserved */
	uint8_t reserved0[7];
	/** Tag */
	struct srp_tag tag;
} __attribute__ (( packed ));

/*****************************************************************************
 *
 * Login request
 *
 *****************************************************************************
 */

/** An SRP login request information unit */
struct srp_login_req {
	/** Information unit type
	 *
	 * This must be @c SRP_LOGIN_REQ
	 */
	uint8_t type;
	/** Reserved */
	uint8_t reserved0[7];
	/** Tag */
	struct srp_tag tag;
	/** Requested maximum initiator to target IU length */
	uint32_t max_i_t_iu_len;
	/** Reserved */
	uint8_t reserved1[4];
	/** Required buffer formats
	 *
	 * This is the bitwise OR of one or more @c
	 * SRP_LOGIN_REQ_FMT_XXX constants.
	 */
	uint16_t required_buffer_formats;
	/** Flags
	 *
	 * This is the bitwise OR of zero or more @c
	 * SRP_LOGIN_REQ_FLAG_XXX and @c SRP_LOGIN_REQ_MCA_XXX
	 * constants.
	 */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved2[5];
	/** Initiator and target port identifiers */
	struct srp_port_ids port_ids;
} __attribute__ (( packed ));

/** Type of an SRP login request */
#define SRP_LOGIN_REQ 0x00

/** Require indirect data buffer descriptor format */
#define SRP_LOGIN_REQ_FMT_IDBD 0x04

/** Require direct data buffer descriptor format */
#define SRP_LOGIN_REQ_FMT_DDBD 0x02

/** Use solicited notification for asynchronous events */
#define SRP_LOGIN_REQ_FLAG_AESOLNT 0x40

/** Use solicited notification for credit request */
#define SRP_LOGIN_REQ_FLAG_CRSOLNT 0x20

/** Use solicited notification for logouts */
#define SRP_LOGIN_REQ_FLAG_LOSOLNT 0x10

/** Multi-channel action mask */
#define SRP_LOGIN_REQ_MCA_MASK 0x03

/** Single RDMA channel operation */
#define SRP_LOGIN_REQ_MCA_SINGLE_CHANNEL 0x00

/** Multiple independent RDMA channel operation */
#define SRP_LOGIN_REQ_MCA_MULTIPLE_CHANNELS 0x01

/*****************************************************************************
 *
 * Login response
 *
 *****************************************************************************
 */

/** An SRP login response */
struct srp_login_rsp {
	/** Information unit type
	 *
	 * This must be @c SRP_LOGIN_RSP
	 */
	uint8_t type;
	/** Reserved */
	uint8_t reserved0[3];
	/** Request limit delta */
	uint32_t request_limit_delta;
	/** Tag */
	struct srp_tag tag;
	/** Maximum initiator to target IU length */
	uint32_t max_i_t_iu_len;
	/** Maximum target to initiator IU length */
	uint32_t max_t_i_iu_len;
	/** Supported buffer formats
	 *
	 * This is the bitwise OR of one or more @c
	 * SRP_LOGIN_RSP_FMT_XXX constants.
	 */
	uint16_t supported_buffer_formats;
	/** Flags
	 *
	 * This is the bitwise OR of zero or more @c
	 * SRP_LOGIN_RSP_FLAG_XXX and @c SRP_LOGIN_RSP_MCR_XXX
	 * constants.
	 */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved1[25];
} __attribute__ (( packed ));

/** Type of an SRP login response */
#define SRP_LOGIN_RSP 0xc0

/** Indirect data buffer descriptor format supported */
#define SRP_LOGIN_RSP_FMT_IDBD 0x04

/** Direct data buffer descriptor format supported */
#define SRP_LOGIN_RSP_FMT_DDBD 0x02

/** Solicited notification is supported */
#define SRP_LOGIN_RSP_FLAG_SOLNTSUP 0x10

/** Multi-channel result mask */
#define SRP_LOGIN_RSP_MCR_MASK 0x03

/** No existing RDMA channels were associated with the same I_T nexus */
#define SRP_LOGIN_RSP_MCR_NO_EXISTING_CHANNELS 0x00

/** One or more existing RDMA channels were terminated */
#define SRP_LOGIN_RSP_MCR_EXISTING_CHANNELS_TERMINATED 0x01

/** One or more existing RDMA channels continue to operate independently */
#define SRP_LOGIN_RSP_MCR_EXISTING_CHANNELS_CONTINUE 0x02

/*****************************************************************************
 *
 * Login rejection
 *
 *****************************************************************************
 */

/** An SRP login rejection */
struct srp_login_rej {
	/** Information unit type
	 *
	 * This must be @c SRP_LOGIN_REJ
	 */
	uint8_t type;
	/** Reserved */
	uint8_t reserved0[3];
	/** Reason
	 *
	 * This is a @c SRP_LOGIN_REJ_REASON_XXX constant.
	 */
	uint32_t reason;
	/** Tag */
	struct srp_tag tag;
	/** Reserved */
	uint8_t reserved1[8];
	/** Supported buffer formats
	 *
	 * This is the bitwise OR of one or more @c
	 * SRP_LOGIN_REJ_FMT_XXX constants.
	 */
	uint16_t supported_buffer_formats;
	/** Reserved */
	uint8_t reserved2[6];
} __attribute__ (( packed ));

/** Type of an SRP login rejection */
#define SRP_LOGIN_REJ 0xc2

/** Unable to establish RDMA channel, no reason specified */
#define SRP_LOGIN_REJ_REASON_UNKNOWN 0x00010000UL

/** Insufficient RDMA channel resources */
#define SRP_LOGIN_REJ_REASON_INSUFFICIENT_RESOURCES 0x00010001UL

/** Requested maximum initiator to target IU length value too large */
#define SRP_LOGIN_REJ_REASON_BAD_MAX_I_T_IU_LEN 0x00010002UL

/** Unable to associate RDMA channel with specified I_T nexus */
#define SRP_LOGIN_REJ_REASON_CANNOT_ASSOCIATE 0x00010003UL

/** One or more requested data buffer descriptor formats are not supported */
#define SRP_LOGIN_REJ_REASON_UNSUPPORTED_BUFFER_FORMAT 0x00010004UL

/** SRP target port does not support multiple RDMA channels per I_T nexus */
#define SRP_LOGIN_REJ_REASON_NO_MULTIPLE_CHANNELS 0x00010005UL

/** RDMA channel limit reached for this initiator */
#define SRP_LOGIN_REJ_REASON_NO_MORE_CHANNELS 0x00010006UL

/** Indirect data buffer descriptor format supported */
#define SRP_LOGIN_REJ_FMT_IDBD 0x04

/** Direct data buffer descriptor format supported */
#define SRP_LOGIN_REJ_FMT_DDBD 0x02

/*****************************************************************************
 *
 * Initiator logout
 *
 *****************************************************************************
 */

/** An SRP initiator logout request */
struct srp_i_logout {
	/** Information unit type
	 *
	 * This must be @c SRP_I_LOGOUT
	 */
	uint8_t type;
	/** Reserved */
	uint8_t reserved0[7];
	/** Tag */
	struct srp_tag tag;
} __attribute__ (( packed ));

/** Type of an SRP initiator logout request */
#define SRP_I_LOGOUT 0x03

/*****************************************************************************
 *
 * Target logout
 *
 *****************************************************************************
 */

/** An SRP target logout request */
struct srp_t_logout {
	/** Information unit type
	 *
	 * This must be @c SRP_T_LOGOUT
	 */
	uint8_t type;
	/** Flags
	 *
	 * This is the bitwise OR of zero or more @c
	 * SRP_T_LOGOUT_FLAG_XXX constants.
	 */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved0[2];
	/** Reason
	 *
	 * This is a @c SRP_T_LOGOUT_REASON_XXX constant.
	 */
	uint32_t reason;
	/** Tag */
	struct srp_tag tag;
} __attribute__ (( packed ));

/** Type of an SRP target logout request */
#define SRP_T_LOGOUT 0x80

/** The initiator specified solicited notification of logouts */
#define SRP_T_LOGOUT_FLAG_SOLNT 0x01

/** No reason specified */
#define SRP_T_LOGOUT_REASON_UNKNOWN 0x00000000UL

/** Inactive RDMA channel (reclaiming resources) */
#define SRP_T_LOGOUT_REASON_INACTIVE 0x00000001UL

/** Invalid information unit type code received by SRP target port */
#define SRP_T_LOGOUT_REASON_INVALID_TYPE 0x00000002UL

/** SRP initiator port sent response with no corresponding request */
#define SRP_T_LOGOUT_REASON_SPURIOUS_RESPONSE 0x00000003UL

/** RDMA channel disconnected due to multi-channel action code in new login */
#define SRP_T_LOGOUT_REASON_MCA 0x00000004UL

/** Unsuppported format code value specified in data-out buffer descriptor */
#define SRP_T_LOGOUT_UNSUPPORTED_DATA_OUT_FORMAT 0x00000005UL

/** Unsuppported format code value specified in data-in buffer descriptor */
#define SRP_T_LOGOUT_UNSUPPORTED_DATA_IN_FORMAT 0x00000006UL

/** Invalid length for IU type */
#define SRP_T_LOGOUT_INVALID_IU_LEN 0x00000008UL

/*****************************************************************************
 *
 * Task management
 *
 *****************************************************************************
 */

/** An SRP task management request */
struct srp_tsk_mgmt {
	/** Information unit type
	 *
	 * This must be @c SRP_TSK_MGMT
	 */
	uint8_t type;
	/** Flags
	 *
	 * This is the bitwise OR of zero or more
	 * @c SRP_TSK_MGMT_FLAG_XXX constants.
	 */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved0[6];
	/** Tag */
	struct srp_tag tag;
	/** Reserved */
	uint8_t reserved1[4];
	/** Logical unit number */
	struct scsi_lun lun;
	/** Reserved */
	uint8_t reserved2[2];
	/** Task management function
	 *
	 * This is a @c SRP_TASK_MGMT_FUNC_XXX constant
	 */
	uint8_t function;
	/** Reserved */
	uint8_t reserved3[1];
	/** Tag of task to be managed */
	struct srp_tag managed_tag;
	/** Reserved */
	uint8_t reserved4[8];
} __attribute__ (( packed ));

/** Type of an SRP task management request */
#define SRP_TSK_MGMT 0x01

/** Use solicited notification for unsuccessful completions */
#define SRP_TSK_MGMT_FLAG_UCSOLNT 0x04

/** Use solicited notification for successful completions */
#define SRP_TSK_MGMT_FLAG_SCSOLNT 0x02

/** The task manager shall perform an ABORT TASK function */
#define SRP_TSK_MGMT_FUNC_ABORT_TASK 0x01

/** The task manager shall perform an ABORT TASK SET function */
#define SRP_TSK_MGMT_FUNC_ABORT_TASK_SET 0x02

/** The task manager shall perform a CLEAR TASK SET function */
#define SRP_TSK_MGMT_FUNC_CLEAR_TASK_SET 0x04

/** The task manager shall perform a LOGICAL UNIT RESET function */
#define SRP_TSK_MGMT_FUNC_LOGICAL_UNIT_RESET 0x08

/** The task manager shall perform a CLEAR ACA function */
#define SRP_TSK_MGMT_FUNC_CLEAR_ACA 0x40

/*****************************************************************************
 *
 * SCSI command
 *
 *****************************************************************************
 */

/** An SRP SCSI command */
struct srp_cmd {
	/** Information unit type
	 *
	 * This must be @c SRP_CMD
	 */
	uint8_t type;
	/** Flags
	 *
	 * This is the bitwise OR of zero or more @c SRP_CMD_FLAG_XXX
	 * constants.
	 */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved0[3];
	/** Data buffer descriptor formats
	 *
	 * This is the bitwise OR of one @c SRP_CMD_DO_FMT_XXX and one @c
	 * SRP_CMD_DI_FMT_XXX constant.
	 */
	uint8_t data_buffer_formats;
	/** Data-out buffer descriptor count */
	uint8_t data_out_buffer_count;
	/** Data-in buffer descriptor count */
	uint8_t data_in_buffer_count;
	/** Tag */
	struct srp_tag tag;
	/** Reserved */
	uint8_t reserved1[4];
	/** Logical unit number */
	struct scsi_lun lun;
	/** Reserved */
	uint8_t reserved2[1];
	/** Task attribute
	 *
	 * This is a @c SRP_CMD_TASK_ATTR_XXX constant.
	 */
	uint8_t task_attr;
	/** Reserved */
	uint8_t reserved3[1];
	/** Additional CDB length */
	uint8_t additional_cdb_len;
	/** Command data block */
	union scsi_cdb cdb;
} __attribute__ (( packed ));

/** Type of an SRP SCSI command */
#define SRP_CMD 0x02

/** Use solicited notification for unsuccessful completions */
#define SRP_CMD_FLAG_UCSOLNT 0x04

/** Use solicited notification for successful completions */
#define SRP_CMD_FLAG_SCSOLNT 0x02

/** Data-out buffer format mask */
#define SRP_CMD_DO_FMT_MASK 0xf0

/** Direct data-out buffer format */
#define SRP_CMD_DO_FMT_DIRECT 0x10

/** Indirect data-out buffer format */
#define SRP_CMD_DO_FMT_INDIRECT 0x20

/** Data-in buffer format mask */
#define SRP_CMD_DI_FMT_MASK 0x0f

/** Direct data-in buffer format */
#define SRP_CMD_DI_FMT_DIRECT 0x01

/** Indirect data-in buffer format */
#define SRP_CMD_DI_FMT_INDIRECT 0x02

/** Use the rules for a simple task attribute */
#define SRP_CMD_TASK_ATTR_SIMPLE 0x00

/** Use the rules for a head of queue task attribute */
#define SRP_CMD_TASK_ATTR_QUEUE_HEAD 0x01

/** Use the rules for an ordered task attribute */
#define SRP_CMD_TASK_ATTR_ORDERED 0x02

/** Use the rules for an automatic contingent allegiance task attribute */
#define SRP_CMD_TASK_ATTR_AUTOMATIC_CONTINGENT_ALLEGIANCE 0x08

/** An SRP memory descriptor */
struct srp_memory_descriptor {
	/** Virtual address */
	uint64_t address;
	/** Memory handle */
	uint32_t handle;
	/** Data length */
	uint32_t len;
} __attribute__ (( packed ));

/*****************************************************************************
 *
 * SCSI response
 *
 *****************************************************************************
 */

/** An SRP SCSI response */
struct srp_rsp {
	/** Information unit type
	 *
	 * This must be @c SRP_RSP
	 */
	uint8_t type;
	/** Flags
	 *
	 * This is the bitwise OR of zero or more @c SRP_RSP_FLAG_XXX
	 * constants.
	 */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved0[2];
	/** Request limit delta */
	uint32_t request_limit_delta;
	/** Tag */
	struct srp_tag tag;
	/** Reserved */
	uint8_t reserved1[2];
	/** Valid fields
	 *
	 * This is the bitwise OR of zero or more @c SRP_RSP_VALID_XXX
	 * constants.
	 */
	uint8_t valid;
	/** Status
	 *
	 * This is the SCSI status code.
	 */
	uint8_t status;
	/** Data-out residual count */
	uint32_t data_out_residual_count;
	/** Data-in residual count */
	uint32_t data_in_residual_count;
	/** Sense data list length */
	uint32_t sense_data_len;
	/** Response data list length */
	uint32_t response_data_len;
} __attribute__ (( packed ));

/** Type of an SRP SCSI response */
#define SRP_RSP 0xc1

/** The initiator specified solicited notification of this response */
#define SRP_RSP_FLAG_SOLNT 0x01

/** Data-in residual count field is valid and represents an underflow */
#define SRP_RSP_VALID_DIUNDER 0x20

/** Data-in residual count field is valid and represents an overflow */
#define SRP_RSP_VALID_DIOVER 0x10

/** Data-out residual count field is valid and represents an underflow */
#define SRP_RSP_VALID_DOUNDER 0x08

/** Data-out residual count field is valid and represents an overflow */
#define SRP_RSP_VALID_DOOVER 0x04

/** Sense data list length field is valid */
#define SRP_RSP_VALID_SNSVALID 0x02

/** Response data list length field is valid */
#define SRP_RSP_VALID_RSPVALID 0x01

/**
 * Get response data portion of SCSI response
 *
 * @v rsp			SCSI response
 * @ret response_data		Response data, or NULL if not present
 */
static inline void * srp_rsp_response_data ( struct srp_rsp *rsp ) {
	return ( ( rsp->valid & SRP_RSP_VALID_RSPVALID ) ?
		 ( ( ( void * ) rsp ) + sizeof ( *rsp ) ) : NULL );
}

/**
 * Get length of response data portion of SCSI response
 *
 * @v rsp			SCSI response
 * @ret response_data_len	Response data length
 */
static inline size_t srp_rsp_response_data_len ( struct srp_rsp *rsp ) {
	return ( ( rsp->valid & SRP_RSP_VALID_RSPVALID ) ?
		 ntohl ( rsp->response_data_len ) : 0 );
}

/**
 * Get sense data portion of SCSI response
 *
 * @v rsp			SCSI response
 * @ret sense_data		Sense data, or NULL if not present
 */
static inline void * srp_rsp_sense_data ( struct srp_rsp *rsp ) {
	return ( ( rsp->valid & SRP_RSP_VALID_SNSVALID ) ?
		 ( ( ( void * ) rsp ) + sizeof ( *rsp ) +
		   srp_rsp_response_data_len ( rsp ) ) : NULL );
}

/**
 * Get length of sense data portion of SCSI response
 *
 * @v rsp			SCSI response
 * @ret sense_data_len		Sense data length
 */
static inline size_t srp_rsp_sense_data_len ( struct srp_rsp *rsp ) {
	return ( ( rsp->valid & SRP_RSP_VALID_SNSVALID ) ?
		 ntohl ( rsp->sense_data_len ) : 0 );
}

/*****************************************************************************
 *
 * Credit request
 *
 *****************************************************************************
 */

/** An SRP credit request */
struct srp_cred_req {
	/** Information unit type
	 *
	 * This must be @c SRP_CRED_REQ
	 */
	uint8_t type;
	/** Flags
	 *
	 * This is the bitwise OR of zero or more
	 * @c SRP_CRED_REQ_FLAG_XXX constants.
	 */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved0[2];
	/** Request limit delta */
	uint32_t request_limit_delta;
	/** Tag */
	struct srp_tag tag;
} __attribute__ (( packed ));

/** Type of an SRP credit request */
#define SRP_CRED_REQ 0x81

/** The initiator specified solicited notification of credit requests */
#define SRP_CRED_REQ_FLAG_SOLNT 0x01

/*****************************************************************************
 *
 * Credit response
 *
 *****************************************************************************
 */

/** An SRP credit response */
struct srp_cred_rsp {
	/** Information unit type
	 *
	 * This must be @c SRP_CRED_RSP
	 */
	uint8_t type;
	/** Reserved */
	uint8_t reserved0[7];
	/** Tag */
	struct srp_tag tag;
} __attribute__ (( packed ));

/** Type of an SRP credit response */
#define SRP_CRED_RSP 0x41

/*****************************************************************************
 *
 * Asynchronous event request
 *
 *****************************************************************************
 */

/** An SRP asynchronous event request */
struct srp_aer_req {
	/** Information unit type
	 *
	 * This must be @c SRP_AER_REQ
	 */
	uint8_t type;
	/** Flags
	 *
	 * This is the bitwise OR of zero or more @c
	 * SRP_AER_REQ_FLAG_XXX constants.
	 */
	uint8_t flags;
	/** Reserved */
	uint8_t reserved0[2];
	/** Request limit delta */
	uint32_t request_limit_delta;
	/** Tag */
	struct srp_tag tag;
	/** Reserved */
	uint8_t reserved1[4];
	/** Logical unit number */
	struct scsi_lun lun;
	/** Sense data list length */
	uint32_t sense_data_len;
	/** Reserved */
	uint8_t reserved2[4];
} __attribute__ (( packed ));

/** Type of an SRP asynchronous event request */
#define SRP_AER_REQ 0x82

/** The initiator specified solicited notification of asynchronous events */
#define SRP_AER_REQ_FLAG_SOLNT 0x01

/**
 * Get sense data portion of asynchronous event request
 *
 * @v aer_req			SRP asynchronous event request
 * @ret sense_data		Sense data
 */
static inline __always_inline void *
srp_aer_req_sense_data ( struct srp_aer_req *aer_req ) {
	return ( ( ( void * ) aer_req ) + sizeof ( *aer_req ) );
}

/**
 * Get length of sense data portion of asynchronous event request
 *
 * @v aer_req			SRP asynchronous event request
 * @ret sense_data_len		Sense data length
 */
static inline __always_inline size_t
srp_aer_req_sense_data_len ( struct srp_aer_req *aer_req ) {
	return ( ntohl ( aer_req->sense_data_len ) );
}

/*****************************************************************************
 *
 * Asynchronous event response
 *
 *****************************************************************************
 */

/** An SRP asynchronous event response */
struct srp_aer_rsp {
	/** Information unit type
	 *
	 * This must be @c SRP_AER_RSP
	 */
	uint8_t type;
	/** Reserved */
	uint8_t reserved0[7];
	/** Tag */
	struct srp_tag tag;
} __attribute__ (( packed ));

/** Type of an SRP asynchronous event response */
#define SRP_AER_RSP 0x42

/*****************************************************************************
 *
 * Information units
 *
 *****************************************************************************
 */

/** Maximum length of any initiator-to-target IU that we will send
 *
 * The longest IU is a SRP_CMD with no additional CDB and two direct
 * data buffer descriptors, which comes to 80 bytes.
 */
#define SRP_MAX_I_T_IU_LEN 80

/*****************************************************************************
 *
 * SRP device
 *
 *****************************************************************************
 */

struct srp_device;

/** An SRP transport type */
struct srp_transport_type {
	/** Length of transport private data */
	size_t priv_len;
	/** Parse root path
	 *
	 * @v srp		SRP device
	 * @v root_path		Root path
	 * @ret 		Return status code
	 */
	int ( * parse_root_path ) ( struct srp_device *srp,
				    const char *root_path );
	/** Connect SRP session
	 *
	 * @v srp		SRP device
	 * @ret rc		Return status code
	 *
	 * This method should open the underlying socket.
	 */
	int ( * connect ) ( struct srp_device *srp );
};

/** An SRP device */
struct srp_device {
	/** Reference count */
	struct refcnt refcnt;

	/** Initiator and target port IDs */
	struct srp_port_ids port_ids;
	/** Logical unit number */
	struct scsi_lun lun;
	/** Memory handle */
	uint32_t memory_handle;

	/** Current state
	 *
	 * This is the bitwise-OR of zero or more @c SRP_STATE_XXX
	 * flags.
	 */
	unsigned int state;
	/** Retry counter */
	unsigned int retry_count;
	/** Current SCSI command */
	struct scsi_command *command;

	/** Underlying data transfer interface */
	struct xfer_interface socket;

	/** Transport type */
	struct srp_transport_type *transport;
	/** Transport private data */
	char transport_priv[0];
};

/**
 * Get SRP transport private data
 *
 * @v srp		SRP device
 * @ret priv		SRP transport private data
 */
static inline __always_inline void *
srp_transport_priv ( struct srp_device *srp ) {
	return ( ( void * ) srp->transport_priv );
}

/** SRP state flags */
enum srp_state {
	/** Underlying socket is open */
	SRP_STATE_SOCKET_OPEN = 0x0001,
	/** Session is logged in */
	SRP_STATE_LOGGED_IN = 0x0002,
};

/** Maximum number of SRP retry attempts */
#define SRP_MAX_RETRIES 3

extern int srp_attach ( struct scsi_device *scsi, const char *root_path );
extern void srp_detach ( struct scsi_device *scsi );

#endif /* _GPXE_SRP_H */
