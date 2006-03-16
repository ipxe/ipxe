#ifndef __mt23108_h__
#define __mt23108_h__

#include "MT23108_PRM.h"
#include "ib_mad.h"

#define TAVOR_DEVICE_ID		0x5a44
#define TAVOR_BRIDGE_DEVICE_ID 0x5a46
#define ARTAVOR_DEVICE_ID	0x6278

#define TAVOR_RESET_OFFSET 0xF0010

/*
 * Tavor specific command
 *	Only coomands that are specific to Tavor
 *  and used by the driver are listed here
 */
#define TAVOR_CMD_SYS_EN	0x1
#define TAVOR_CMD_SYS_DIS	0x2

#define TAVOR_CMD_WRITE_MGM		0x26
#define TAVOR_CMD_MOD_STAT_CFG	0x34
#define TAVOR_CMD_QUERY_DEV_LIM   0x003
#define TAVOR_CMD_QUERY_FW		0x004

/*
 * Tavor specific event types
 *	Only event types that are specific to Tavor
 *  and are used by the driver are listed here
 */
#define TAVOR_IF_EV_TYPE_OVERRUN	0x0F

/*
 * EQ doorbel commands
 */
#define EQ_DBELL_CMD_INC_CONS_IDX 1	/* increment Consumer_indx by one */
#define EQ_DBELL_CMD_ARM_EQ       2	/* Request notifcation for next event (Arm EQ) */
#define EQ_DBELL_CMD_DISARM_CQ    3	/* Disarm CQ (CQ number is specified in EQ_param) */
#define EQ_DBELL_CMD_SET_CONS_IDX 4	/* set Consumer_indx to value of EQ_param */
#define EQ_DBELL_CMD_ALWAYS_ARM   5	/* move EQ to Always Armed state */

/*
 * CQ doorbel commands
 */
#define CQ_DBELL_CMD_INC_CONS_IDX 1
#define CQ_DBELL_CMD_REQ_NOTIF_SOL_UNSOL 2
#define CQ_DBELL_CMD_REQ_NOTIF_SOL 3
#define CQ_DBELL_CMD_SET_CONS_IDX 4
#define CQ_DBELL_CMD_REQ_NOTIF_MULT 5

#define INPRM_BUF_SZ 0x200
#define INPRM_BUF_ALIGN 16
#define OUTPRM_BUF_SZ 0x200
#define OUTPRM_BUF_ALIGN 16

/*
 *  sizes of parameter blocks used in certain
 *	commands.
 *  TODO: replace them with sizeof
 *  operators of the appropriate structs
 */
#define SW2HW_MPT_IBUF_SZ	 MT_STRUCT_SIZE(tavorprm_mpt_st)
#define SW2HW_EQ_IBUF_SZ	 MT_STRUCT_SIZE(tavorprm_eqc_st)
#define INIT_IB_IBUF_SZ		 0x100
#define SW2HW_CQ_IBUF_SZ	 0x40
#define QPCTX_IBUF_SZ		 0x200

#define EQN 0
#define UAR_IDX 1

#define QPC_OFFSET 0
#define CQC_OFFSET (QPC_OFFSET + 0x100000)
#define EQPC_OFFSET (CQC_OFFSET + 0x100000)
#define EQC_OFFSET (EQPC_OFFSET + 0x100000)
#define MC_BASE_OFFSET (EQC_OFFSET + 0x100000)
#define MPT_BASE_OFFSET (MC_BASE_OFFSET + 0x100000)
#define MTT_BASE_OFFSET (MPT_BASE_OFFSET + 0x100000)

#define LOG2_QPS 7
#define LOG2_CQS 8
#define LOG2_EQS 6
#define LOG2_MC_ENTRY 6		/* 8 QPs per group */
#define LOG2_MC_GROUPS 3	/* 8 groups */
#define LOG2_MPT_ENTRIES 5

#define LOG2_EQ_SZ 5
#define LOG2_CQ_SZ 5

#define NUM_PORTS 2

#define EQE_OWNER_SW 0
#define EQE_OWNER_HW 1

#define OWNER_HW 1
#define OWNER_SW 0

#define POST_RCV_OFFSET 0x18
#define POST_SND_OFFSET 0x10
#define CQ_DBELL_OFFSET 0x20
#define EQ_DBELL_OFFSET 0x28

#define CQE_ERROR_OPCODE 0xfe

#define MAX_GATHER 1		/* max gather entries used in send */
#define MAX_SCATTER 2

#define LOG2_MADS_SND_CQ_SZ LOG2_CQ_SZ
#define LOG2_MADS_RCV_CQ_SZ LOG2_CQ_SZ
#define LOG2_IPOIB_SND_CQ_SZ LOG2_CQ_SZ
#define LOG2_IPOIB_RCV_CQ_SZ LOG2_CQ_SZ

#define NUM_MADS_SND_CQES (1<<LOG2_MADS_SND_CQ_SZ)
#define NUM_MADS_RCV_CQES (1<<LOG2_MADS_RCV_CQ_SZ)
#define NUM_IPOIB_SND_CQES (1<<LOG2_IPOIB_SND_CQ_SZ)
#define NUM_IPOIB_RCV_CQES (1<<LOG2_IPOIB_RCV_CQ_SZ)

#define NUM_MADS_RCV_WQES 3
#define NUM_IPOIB_RCV_WQES 8

#if NUM_MADS_RCV_WQES > NUM_IPOIB_RCV_WQES
#define MAX_RCV_WQES NUM_MADS_RCV_WQES
#else
#define MAX_RCV_WQES NUM_IPOIB_RCV_WQES
#endif

#define NUM_MADS_SND_WQES 2
#define NUM_IPOIB_SND_WQES 2

#if NUM_MADS_SND_WQES > NUM_IPOIB_SND_WQES
#define MAX_SND_WQES NUM_MADS_SND_WQES
#else
#define MAX_SND_WQES NUM_IPOIB_SND_WQES
#endif

struct ib_buffers_st {
	__u8 send_mad_buf[NUM_MADS_SND_WQES][MAD_BUF_SZ];
	__u8 rcv_mad_buf[NUM_MADS_RCV_WQES][MAD_BUF_SZ + GRH_SIZE];
	__u8 ipoib_rcv_buf[NUM_IPOIB_RCV_WQES][IPOIB_RCV_BUF_SZ + GRH_SIZE];
	__u8 ipoib_rcv_grh_buf[NUM_IPOIB_RCV_WQES][IPOIB_RCV_BUF_SZ];
	__u8 send_ipoib_buf[NUM_IPOIB_SND_WQES][IPOIB_SND_BUF_SZ];
};

struct pcidev {
	unsigned long bar[6];
	__u32 dev_config_space[64];
	struct pci_device *dev;
	__u8 bus;
	__u8 devfn;
};

struct dev_pci_struct {
	struct pcidev dev;
	struct pcidev br;
	void *cr_space;
	void *uar;
};

struct eq_st {
	__u8 eqn;
	__u32 cons_idx;
	__u32 eq_size;
	struct eqe_t *eq_buf;
};

struct udav_st {
	union ud_av_u *av_array;
	__u8 udav_next_free;
};

#if 0
struct udavtable_memory_parameters_st {
	__u32 lkey;
	__u32 pd:24;
	__u32 r0:5;
	__u32 xlation_en:1;
	__u32 r1:2;
} __attribute__ ((packed));

struct multicast_parameters_st {
	__u32 mc_base_addr_h;
	__u32 mc_base_addr_l;
	__u32 r0[2];
	__u32 log_mc_table_entry_sz:16;
	__u32 r1:16;
	__u32 mc_table_hash_sz:17;
	__u32 r2:15;
	__u32 log_mc_table_sz:5;
	__u32 r3:19;
	__u32 mc_hash_fn:3;
	__u32 r4:5;
	__u32 r5;
} __attribute__ ((packed));

struct tpt_parameters_st {
	__u32 mpt_base_addr_h;
	__u32 mpt_base_addr_l;

	__u32 log_mpt_sz:6;
	__u32 r0:2;
	__u32 pfto:5;
	__u32 r1:3;
	__u32 mtt_segment_size:3;
	__u32 r2:13;

	__u32 mtt_version:8;
	__u32 r3:24;

	__u32 mtt_base_addr_h;
	__u32 mtt_base_addr_l;
	__u32 r4[2];
} __attribute__ ((packed));

struct uar_parameters_st {
	__u32 uar_base_addr_h;
	__u32 uar_base_addr_l;	/* 12 lsbs must be zero */
	__u32 uar_page_sz:8;
	__u32 r1:24;
	__u32 r2;
	__u32 uar_scratch_base_addr_h;
	__u32 uar_scratch_base_addr_l;
	__u32 r3[3];
} __attribute__ ((packed));

struct comp_event_data_st {
	__u32 cqn:24;
	__u32 r1:8;
	__u32 r2[5];
} __attribute__ ((packed));

struct qp_event_data_st {
	__u32 qpn_een:24;
	__u32 r1:8;
	__u32 r2;
	__u32 r3:28;
	__u32 e_q:1;
	__u32 r4:3;
	__u32 r5[3];
} __attribute__ ((packed));

struct port_state_change_event_data_st {
	__u32 r0[2];
	__u32 r1:28;
	__u32 port:2;
	__u32 r2:2;
	__u32 r3[3];
} __attribute__ ((packed));
#endif

struct eqe_t {
	__u8 raw[MT_STRUCT_SIZE(tavorprm_event_queue_entry_st)];
} __attribute__ ((packed));

enum qp_state_e {
	QP_STATE_RST = 0,
	QP_STATE_INIT = 1,
	QP_STATE_RTR = 2,
	QP_STATE_RTS = 3,
	QP_STATE_SQEr = 4,
	QP_STATE_SQD = 5,
	QP_STATE_ERR = 6,
	QP_STATE_SQDING = 7,
	QP_STATE_SUSPEND = 9
};

struct memory_pointer_st {
	__u32 byte_count;
	__u32 lkey;
	__u32 local_addr_h;
	__u32 local_addr_l;
} __attribute__ ((packed));

/* receive wqe descriptor */
struct recv_wqe_st {
	/* part referenced by hardware */
	__u8 next[MT_STRUCT_SIZE(wqe_segment_next_st)];
	__u8 control[MT_STRUCT_SIZE(wqe_segment_ctrl_recv_st)];
	struct memory_pointer_st mpointer[MAX_SCATTER];
} __attribute__ ((packed));

struct recv_wqe_cont_st {
	struct recv_wqe_st wqe;

	struct udqp_st *qp;	/* qp this wqe is used with */
} __attribute__ ((packed));

#define RECV_WQE_U_ALIGN 64
union recv_wqe_u {
	__u8 align[(sizeof(struct recv_wqe_cont_st) + RECV_WQE_U_ALIGN - 1) & (~(RECV_WQE_U_ALIGN - 1))];	/* this ensures proper alignment */
	struct recv_wqe_st wqe;
	struct recv_wqe_cont_st wqe_cont;
} __attribute__ ((packed));

struct recv_doorbell_st {
	__u8 raw[MT_STRUCT_SIZE(tavorprm_receive_doorbell_st)];
} __attribute__ ((packed));

struct send_doorbell_st {
	__u8 raw[MT_STRUCT_SIZE(tavorprm_send_doorbell_st)];
} __attribute__ ((packed));

struct next_control_seg_st {
	__u8 next[MT_STRUCT_SIZE(wqe_segment_next_st)];
	__u8 control[MT_STRUCT_SIZE(wqe_segment_ctrl_send_st)];
} __attribute__ ((packed));

struct ud_seg_st {
	__u32 r1;
	__u32 lkey;
	__u32 av_add_h;
	__u32 av_add_l;
	__u32 r2[4];
	__u32 dest_qp;
	__u32 qkey;
	__u32 r3[2];
} __attribute__ ((packed));

struct ud_send_wqe_st {
	struct next_control_seg_st next;
	struct ud_seg_st udseg;
	struct memory_pointer_st mpointer[MAX_GATHER];
} __attribute__ ((packed));

struct ude_send_wqe_cont_st {
	struct ud_send_wqe_st wqe;

	struct udqp_st *qp;	/* qp this wqe is used with */
} __attribute__ ((packed));

#define UD_SEND_WQE_U_ALIGN 64
union ud_send_wqe_u {
	__u8 align[(sizeof(struct ude_send_wqe_cont_st) + UD_SEND_WQE_U_ALIGN -
		    1) & (~(UD_SEND_WQE_U_ALIGN - 1))];
	struct ude_send_wqe_cont_st wqe_cont;
	struct ud_send_wqe_st wqe;
} __attribute__ ((packed));

#define ADDRESS_VECTOR_ST_ALIGN 64
struct address_vector_st {
	__u8 raw[MT_STRUCT_SIZE(tavorprm_ud_address_vector_st)];
} __attribute__ ((packed));

struct ud_av_st {
	struct address_vector_st av;
	__u32 dest_qp;		/* destination qpn */
	__u8 next_free;
} __attribute__ ((packed));

union ud_av_u {
	__u8 raw[(sizeof(struct ud_av_st) + ADDRESS_VECTOR_ST_ALIGN -
		  1) & (~(ADDRESS_VECTOR_ST_ALIGN - 1))];
	struct ud_av_st ud_av;
} __attribute__ ((packed));

union cqe_st {
	__u8 good_cqe[MT_STRUCT_SIZE(tavorprm_completion_queue_entry_st)];
	__u8 error_cqe[MT_STRUCT_SIZE(tavorprm_completion_with_error_st)];
} __attribute__ ((packed));

struct address_path_st {
	__u8 raw[MT_STRUCT_SIZE(tavorprm_address_path_st)];
};

struct qp_ee_ctx_t {
	__u8 raw[MT_STRUCT_SIZE(tavorprm_queue_pair_ee_context_entry_st)];
} __attribute__ ((packed));

struct qp_ee_state_tarnisition_st {
	__u32 opt_param_mask;
	__u32 r1;
	struct qp_ee_ctx_t ctx;
	__u32 r2[62];
} __attribute__ ((packed));

struct eq_dbell_st {
	__u8 raw[MT_STRUCT_SIZE(tavorprm_eq_cmd_doorbell_st)];
} __attribute__ ((packed));

struct cq_dbell_st {
	__u8 raw[MT_STRUCT_SIZE(tavorprm_cq_cmd_doorbell_st)];
} __attribute__ ((packed));

struct mad_ifc_inprm_st {
	union mad_u mad;
} __attribute__ ((packed));

struct wqe_buf_st {
	struct ud_send_wqe_st *sndq;
	struct recv_wqe_st *rcvq;
};

struct mad_buffer_st {
	void *buf;		/* pointer to a 256 byte buffer */
	__u8 owner;		/* sw or hw ownership BUF_OWNER_SW or BUF_OWNER_HW */
};

struct rcv_buf_st {
	void *buf;
	__u8 busy;
};

struct ib_eqe_st {
	__u8 event_type;
	__u32 cqn;
};

struct cq_st {
	__u32 cqn;
	union cqe_st *cq_buf;
	__u32 cons_idx;
	__u8 num_cqes;
};

struct udqp_st {
	/* cq used by this QP */
	struct cq_st snd_cq;
	struct cq_st rcv_cq;

	/* QP related data */
	__u32 qpn;		/* QP number */

	__u32 qkey;

	__u8 recv_wqe_cur_free;
	__u8 recv_wqe_alloc_idx;
	__u8 max_recv_wqes;
	void *rcv_bufs[MAX_RCV_WQES];
	union recv_wqe_u *rcv_wq;	/* receive work queue */
	struct recv_wqe_st *last_posted_rcv_wqe;

	__u8 snd_wqe_cur_free;
	__u8 snd_wqe_alloc_idx;
	__u8 max_snd_wqes;
	void *snd_bufs[MAX_SND_WQES];
	__u16 send_buf_sz;
	__u16 rcv_buf_sz;
	union ud_send_wqe_u *snd_wq;	/* send work queue */
	struct ud_send_wqe_st *last_posted_snd_wqe;
};

struct device_ib_data_st {
	__u32 mkey;
	__u32 pd;
	__u8 port;
	__u32 qkey;
	struct eq_st eq;
	struct udav_st udav;
	struct udqp_st mads_qp;
	struct udqp_st ipoib_qp;
	void *error_buf_addr;
	__u32 error_buf_size;
};



struct query_fw_st {
	__u16 fw_rev_major;
	__u16 fw_rev_minor;
	__u16 fw_rev_subminor;
	__u32 error_buf_start_h;
	__u32 error_buf_start_l;
	__u32 error_buf_size;
};


struct dev_lim_st {
	__u8 log2_rsvd_qps;
	__u16 qpc_entry_sz;

	__u8 log2_rsvd_srqs;
	__u16 srq_entry_sz;

	__u8 log2_rsvd_ees;
	__u16 eec_entry_sz;

	__u8 log2_rsvd_cqs;
	__u16 cqc_entry_sz;

	__u8 log2_rsvd_mtts;
	__u16 mtt_entry_sz;

	__u8 log2_rsvd_mrws;
	__u16 mpt_entry_sz;

	__u16 eqc_entry_sz;
};

struct init_hca_st {
	__u32 qpc_base_addr_h;
	__u32 qpc_base_addr_l;
	__u8 log_num_of_qp;

	__u32 eec_base_addr_h;
	__u32 eec_base_addr_l;
	__u8 log_num_of_ee;

	__u32 srqc_base_addr_h;
	__u32 srqc_base_addr_l;
	__u8 log_num_of_srq;

	__u32 cqc_base_addr_h;
	__u32 cqc_base_addr_l;
	__u8 log_num_of_cq;

	__u32 eqpc_base_addr_h;
	__u32 eqpc_base_addr_l;

	__u32 eeec_base_addr_h;
	__u32 eeec_base_addr_l;

	__u32 eqc_base_addr_h;
	__u32 eqc_base_addr_l;
	__u8 log_num_of_eq;

	__u32 rdb_base_addr_h;
	__u32 rdb_base_addr_l;

	__u32 mc_base_addr_h;
	__u32 mc_base_addr_l;
	__u16 log_mc_table_entry_sz;
	__u32 mc_table_hash_sz;
	__u8 log_mc_table_sz;

	__u32 mpt_base_addr_h;
	__u32 mpt_base_addr_l;
	__u8 log_mpt_sz;
	__u32 mtt_base_addr_h;
	__u32 mtt_base_addr_l;
	__u8 log_max_uars;
};

static int create_udqp(struct udqp_st *qp);
static int destroy_udqp(struct udqp_st *qp);
static void *get_send_wqe_buf(void *wqe, __u8 index);
static void *get_rcv_wqe_buf(void *wqe, __u8 index);

static struct recv_wqe_st *alloc_rcv_wqe(struct udqp_st *qp);
static int free_wqe(void *wqe);
static int poll_cq(void *cqh, union cqe_st *cqe_p, __u8 * num_cqes);
static int poll_eq(struct ib_eqe_st *ib_eqe_p, __u8 * num_eqes);
static int post_rcv_buf(struct udqp_st *qp, struct recv_wqe_st *rcv_wqe);
static __u32 dev_get_qpn(void *qph);

#endif				/* __mt23108_h__ */
