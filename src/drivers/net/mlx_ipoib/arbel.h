#ifndef _ARBEL_H
#define _ARBEL_H

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

#endif /* _ARBEL_H */
