#ifndef _ARBEL_H
#define _ARBEL_H

/** @file
 *
 * Mellanox Arbel Infiniband HCA driver
 *
 */

/*
 * Hardware constants
 *
 */

#define ARBEL_OPCODE_SEND		0x0a
#define ARBEL_OPCODE_RECV_ERROR		0xfe
#define ARBEL_OPCODE_SEND_ERROR		0xff

/*
 * Wrapper structures for hardware datatypes
 *
 */

struct MLX_DECLARE_STRUCT ( arbelprm_completion_queue_entry );
struct MLX_DECLARE_STRUCT ( arbelprm_completion_with_error );
struct MLX_DECLARE_STRUCT ( arbelprm_cq_ci_db_record );
struct MLX_DECLARE_STRUCT ( arbelprm_qp_db_record );
struct MLX_DECLARE_STRUCT ( arbelprm_send_doorbell );
struct MLX_DECLARE_STRUCT ( arbelprm_ud_address_vector );
struct MLX_DECLARE_STRUCT ( arbelprm_wqe_segment_ctrl_send );
struct MLX_DECLARE_STRUCT ( arbelprm_wqe_segment_data_ptr );
struct MLX_DECLARE_STRUCT ( arbelprm_wqe_segment_next );
struct MLX_DECLARE_STRUCT ( arbelprm_wqe_segment_ud );

/*
 * Composite hardware datatypes
 *
 */

#define ARBELPRM_MAX_GATHER 1

struct arbelprm_ud_send_wqe {
	struct arbelprm_wqe_segment_next next;
	struct arbelprm_wqe_segment_ctrl_send ctrl;
	struct arbelprm_wqe_segment_ud ud;
	struct arbelprm_wqe_segment_data_ptr data[ARBELPRM_MAX_GATHER];
} __attribute__ (( packed ));

union arbelprm_completion_entry {
	struct arbelprm_completion_queue_entry normal;
	struct arbelprm_completion_with_error error;
} __attribute__ (( packed ));

union arbelprm_doorbell_record {
	struct arbelprm_cq_ci_db_record cq_ci;
	struct arbelprm_qp_db_record qp;
} __attribute__ (( packed ));

union arbelprm_doorbell_register {
	struct arbelprm_send_doorbell send;
	uint32_t dword[2];
} __attribute__ (( packed ));

/*
 * gPXE-specific definitions
 *
 */

/** Alignment of Arbel send work queue entries */
#define ARBEL_SEND_WQE_ALIGN 128

/** An Arbel send work queue entry */
union arbel_send_wqe {
	struct arbelprm_ud_send_wqe ud;
	uint8_t force_align[ARBEL_SEND_WQE_ALIGN];
} __attribute__ (( packed ));

/** An Arbel send work queue */
struct arbel_send_work_queue {
	/** Doorbell record number */
	unsigned int doorbell_idx;
	/** Work queue entries */
	union arbel_send_wqe *wqe;
};

/** Alignment of Arbel receive work queue entries */
#define ARBEL_RECV_WQE_ALIGN 64

/** An Arbel receive work queue entry */
union arbel_recv_wqe {
	uint8_t force_align[ARBEL_RECV_WQE_ALIGN];
} __attribute__ (( packed ));

/** An Arbel receive work queue */
struct arbel_recv_work_queue {
	/** Doorbell record number */
	unsigned int doorbell_idx;
	/** Work queue entries */
	union arbel_recv_wqe *wqe;
};

/** An Arbel completion queue */
struct arbel_completion_queue {
	/** Doorbell record number */
	unsigned int doorbell_idx;
	/** Completion queue entries */
	union arbelprm_completion_entry *cqe;
};

/** An Arbel device */
struct arbel {
	/** User Access Region */
	void *uar;
	/** Doorbell records */
	union arbelprm_doorbell_record *db_rec;
};

#endif /* _ARBEL_H */
