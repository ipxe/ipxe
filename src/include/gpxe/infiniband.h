#ifndef _GPXE_INFINIBAND_H
#define _GPXE_INFINIBAND_H

/** @file
 *
 * Infiniband protocol
 *
 */

#include <stdint.h>
#include <gpxe/device.h>

/** Subnet administrator QPN */
#define IB_SA_QPN 1

/** Broadcast QPN */
#define IB_BROADCAST_QPN 0xffffffUL

/** Subnet administrator queue key */
#define IB_GLOBAL_QKEY 0x80010000UL

/** An Infiniband Global Identifier */
struct ib_gid {
	union {
		uint8_t bytes[16];
		uint16_t words[8];
		uint32_t dwords[4];
	} u;
};

/** An Infiniband Global Route Header */
struct ib_global_route_header {
	/** IP version, traffic class, and flow label
	 *
	 *  4 bits : Version of the GRH
	 *  8 bits : Traffic class
	 * 20 bits : Flow label
	 */
	uint32_t ipver_tclass_flowlabel;
	/** Payload length */
	uint16_t paylen;
	/** Next header */
	uint8_t nxthdr;
	/** Hop limit */
	uint8_t hoplmt;
	/** Source GID */
	struct ib_gid sgid;
	/** Destiniation GID */
	struct ib_gid dgid;
} __attribute__ (( packed ));

struct ib_device;
struct ib_queue_pair;
struct ib_completion_queue;

/** An Infiniband Work Queue */
struct ib_work_queue {
	/** Containing queue pair */
	struct ib_queue_pair *qp;
	/** "Is a send queue" flag */
	int is_send;
	/** Associated completion queue */
	struct ib_completion_queue *cq;
	/** List of work queues on this completion queue */
	struct list_head list;
	/** Number of work queue entries */
	unsigned int num_wqes;
	/** Next work queue entry index
	 *
	 * This is the index of the next entry to be filled (i.e. the
	 * first empty entry).  This value is not bounded by num_wqes;
	 * users must logical-AND with (num_wqes-1) to generate an
	 * array index.
	 */
	unsigned long next_idx;
	/** I/O buffers assigned to work queue */
	struct io_buffer **iobufs;
	/** Driver private data */
	void *drv_priv;
};

/** An Infiniband Queue Pair */
struct ib_queue_pair {
	/** Queue Pair Number */
	unsigned long qpn;
	/** Queue key */
	unsigned long qkey;
	/** Send queue */
	struct ib_work_queue send;
	/** Receive queue */
	struct ib_work_queue recv;
	/** Driver private data */
	void *drv_priv;
	/** Queue owner private data */
	void *owner_priv;
};

/** Infiniband queue pair modification flags */
enum ib_queue_pair_mods {
	IB_MODIFY_QKEY = 0x0001,
};

/** An Infiniband Completion Queue */
struct ib_completion_queue {
	/** Completion queue number */
	unsigned long cqn;
	/** Number of completion queue entries */
	unsigned int num_cqes;
	/** Next completion queue entry index
	 *
	 * This is the index of the next entry to be filled (i.e. the
	 * first empty entry).  This value is not bounded by num_wqes;
	 * users must logical-AND with (num_wqes-1) to generate an
	 * array index.
	 */
	unsigned long next_idx;
	/** List of work queues completing to this queue */
	struct list_head work_queues;
	/** Driver private data */
	void *drv_priv;
};

/** An Infiniband completion */
struct ib_completion {
	/** Syndrome
	 *
	 * If non-zero, then the completion is in error.
	 */
	unsigned int syndrome;
	/** Length */
	size_t len;
};

/** An Infiniband completion handler
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v completion	Completion
 * @v iobuf		I/O buffer
 */
typedef void ( * ib_completer_t ) ( struct ib_device *ibdev,
				    struct ib_queue_pair *qp,
				    struct ib_completion *completion,
				    struct io_buffer *iobuf );

/** An Infiniband Address Vector */
struct ib_address_vector {
	/** Destination Queue Pair */
	unsigned int dest_qp;
	/** Queue key */
	unsigned long qkey;
	/** Destination Local ID */
	unsigned int dlid;
	/** Rate */
	unsigned int rate;
	/** Service level */
	unsigned int sl;
	/** GID is present */
	unsigned int gid_present;
	/** GID */
	struct ib_gid gid;
};

struct ib_mad_hdr;

/**
 * Infiniband device operations
 *
 * These represent a subset of the Infiniband Verbs.
 */
struct ib_device_operations {
	/** Create completion queue
	 *
	 * @v ibdev		Infiniband device
	 * @v cq		Completion queue
	 * @ret rc		Return status code
	 */
	int ( * create_cq ) ( struct ib_device *ibdev,
			      struct ib_completion_queue *cq );
	/** Destroy completion queue
	 *
	 * @v ibdev		Infiniband device
	 * @v cq		Completion queue
	 */
	void ( * destroy_cq ) ( struct ib_device *ibdev,
				struct ib_completion_queue *cq );
	/** Create queue pair
	 *
	 * @v ibdev		Infiniband device
	 * @v qp		Queue pair
	 * @ret rc		Return status code
	 */
	int ( * create_qp ) ( struct ib_device *ibdev,
			      struct ib_queue_pair *qp );
	/** Modify queue pair
	 *
	 * @v ibdev		Infiniband device
	 * @v qp		Queue pair
	 * @v mod_list		Modification list
	 * @ret rc		Return status code
	 */
	int ( * modify_qp ) ( struct ib_device *ibdev,
			      struct ib_queue_pair *qp,
			      unsigned long mod_list );
	/** Destroy queue pair
	 *
	 * @v ibdev		Infiniband device
	 * @v qp		Queue pair
	 */
	void ( * destroy_qp ) ( struct ib_device *ibdev,
				struct ib_queue_pair *qp );
	/** Post send work queue entry
	 *
	 * @v ibdev		Infiniband device
	 * @v qp		Queue pair
	 * @v av		Address vector
	 * @v iobuf		I/O buffer
	 * @ret rc		Return status code
	 *
	 * If this method returns success, the I/O buffer remains
	 * owned by the queue pair.  If this method returns failure,
	 * the I/O buffer is immediately released; the failure is
	 * interpreted as "failure to enqueue buffer".
	 */
	int ( * post_send ) ( struct ib_device *ibdev,
			      struct ib_queue_pair *qp,
			      struct ib_address_vector *av,
			      struct io_buffer *iobuf );
	/** Post receive work queue entry
	 *
	 * @v ibdev		Infiniband device
	 * @v qp		Queue pair
	 * @v iobuf		I/O buffer
	 * @ret rc		Return status code
	 *
	 * If this method returns success, the I/O buffer remains
	 * owned by the queue pair.  If this method returns failure,
	 * the I/O buffer is immediately released; the failure is
	 * interpreted as "failure to enqueue buffer".
	 */
	int ( * post_recv ) ( struct ib_device *ibdev,
			      struct ib_queue_pair *qp,
			      struct io_buffer *iobuf );
	/** Poll completion queue
	 *
	 * @v ibdev		Infiniband device
	 * @v cq		Completion queue
	 * @v complete_send	Send completion handler
	 * @v complete_recv	Receive completion handler
	 *
	 * The completion handler takes ownership of the I/O buffer.
	 */
	void ( * poll_cq ) ( struct ib_device *ibdev,
			     struct ib_completion_queue *cq,
			     ib_completer_t complete_send,
			     ib_completer_t complete_recv );
	/**
	 * Open port
	 *
	 * @v ibdev		Infiniband device
	 * @ret rc		Return status code
	 */
	int ( * open ) ( struct ib_device *ibdev );
	/**
	 * Close port
	 *
	 * @v ibdev		Infiniband device
	 */
	void ( * close ) ( struct ib_device *ibdev );
	/** Attach to multicast group
	 *
	 * @v ibdev		Infiniband device
	 * @v qp		Queue pair
	 * @v gid		Multicast GID
	 * @ret rc		Return status code
	 */
	int ( * mcast_attach ) ( struct ib_device *ibdev,
				 struct ib_queue_pair *qp,
				 struct ib_gid *gid );
	/** Detach from multicast group
	 *
	 * @v ibdev		Infiniband device
	 * @v qp		Queue pair
	 * @v gid		Multicast GID
	 */
	void ( * mcast_detach ) ( struct ib_device *ibdev,
				  struct ib_queue_pair *qp,
				  struct ib_gid *gid );
	/**
	 * Issue management datagram
	 *
	 * @v ibdev		Infiniband device
	 * @v mad		Management datagram
	 * @v len		Length of management datagram
	 * @ret rc		Return status code
	 */
	int ( * mad ) ( struct ib_device *ibdev, struct ib_mad_hdr *mad,
			size_t len );
};

/** An Infiniband device */
struct ib_device {
	/** Underlying device */
	struct device *dev;
	/** Infiniband operations */
	struct ib_device_operations *op;
	/** Port number */
	unsigned int port;
	/** Link state */
	int link_up;
	/** Port GID */
	struct ib_gid port_gid;
	/** Subnet manager LID */
	unsigned long sm_lid;
	/** Partition key */
	unsigned int pkey;
	/** Driver private data */
	void *drv_priv;
	/** Owner private data */
	void *owner_priv;
};

extern struct ib_completion_queue * ib_create_cq ( struct ib_device *ibdev,
						   unsigned int num_cqes );
extern void ib_destroy_cq ( struct ib_device *ibdev,
			    struct ib_completion_queue *cq );
extern struct ib_queue_pair *
ib_create_qp ( struct ib_device *ibdev, unsigned int num_send_wqes,
	       struct ib_completion_queue *send_cq, unsigned int num_recv_wqes,
	       struct ib_completion_queue *recv_cq, unsigned long qkey );
extern int ib_modify_qp ( struct ib_device *ibdev, struct ib_queue_pair *qp,
			  unsigned long mod_list, unsigned long qkey );
extern void ib_destroy_qp ( struct ib_device *ibdev,
			    struct ib_queue_pair *qp );
extern struct ib_work_queue * ib_find_wq ( struct ib_completion_queue *cq,
					   unsigned long qpn, int is_send );
extern struct ib_device * alloc_ibdev ( size_t priv_size );
extern int register_ibdev ( struct ib_device *ibdev );
extern void unregister_ibdev ( struct ib_device *ibdev );
extern void free_ibdev ( struct ib_device *ibdev );
extern void ib_link_state_changed ( struct ib_device *ibdev );

/**
 * Post send work queue entry
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v av		Address vector
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static inline __attribute__ (( always_inline )) int
ib_post_send ( struct ib_device *ibdev, struct ib_queue_pair *qp,
	       struct ib_address_vector *av, struct io_buffer *iobuf ) {
	return ibdev->op->post_send ( ibdev, qp, av, iobuf );
}

/**
 * Post receive work queue entry
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static inline __attribute__ (( always_inline )) int
ib_post_recv ( struct ib_device *ibdev, struct ib_queue_pair *qp,
	       struct io_buffer *iobuf ) {
	return ibdev->op->post_recv ( ibdev, qp, iobuf );
}

/**
 * Poll completion queue
 *
 * @v ibdev		Infiniband device
 * @v cq		Completion queue
 * @v complete_send	Send completion handler
 * @v complete_recv	Receive completion handler
 */
static inline __attribute__ (( always_inline )) void
ib_poll_cq ( struct ib_device *ibdev, struct ib_completion_queue *cq,
	     ib_completer_t complete_send, ib_completer_t complete_recv ) {
	ibdev->op->poll_cq ( ibdev, cq, complete_send, complete_recv );
}

/**
 * Open port
 *
 * @v ibdev		Infiniband device
 * @ret rc		Return status code
 */
static inline __attribute__ (( always_inline )) int
ib_open ( struct ib_device *ibdev ) {
	return ibdev->op->open ( ibdev );
}

/**
 * Close port
 *
 * @v ibdev		Infiniband device
 */
static inline __attribute__ (( always_inline )) void
ib_close ( struct ib_device *ibdev ) {
	ibdev->op->close ( ibdev );
}

/**
 * Attach to multicast group
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v gid		Multicast GID
 * @ret rc		Return status code
 */
static inline __attribute__ (( always_inline )) int
ib_mcast_attach ( struct ib_device *ibdev, struct ib_queue_pair *qp,
		  struct ib_gid *gid ) {
	return ibdev->op->mcast_attach ( ibdev, qp, gid );
}

/**
 * Detach from multicast group
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v gid		Multicast GID
 */
static inline __attribute__ (( always_inline )) void
ib_mcast_detach ( struct ib_device *ibdev, struct ib_queue_pair *qp,
		  struct ib_gid *gid ) {
	ibdev->op->mcast_detach ( ibdev, qp, gid );
}

/**
 * Issue management datagram
 *
 * @v ibdev		Infiniband device
 * @v mad		Management datagram
 * @v len		Length of management datagram
 * @ret rc		Return status code
 */
static inline __attribute__ (( always_inline )) int
ib_mad ( struct ib_device *ibdev, struct ib_mad_hdr *mad, size_t len ) {
	return ibdev->op->mad ( ibdev, mad, len );
}

/**
 * Set Infiniband work queue driver-private data
 *
 * @v wq		Work queue
 * @v priv		Private data
 */
static inline __attribute__ (( always_inline )) void
ib_wq_set_drvdata ( struct ib_work_queue *wq, void *priv ) {
	wq->drv_priv = priv;
}

/**
 * Get Infiniband work queue driver-private data
 *
 * @v wq		Work queue
 * @ret priv		Private data
 */
static inline __attribute__ (( always_inline )) void *
ib_wq_get_drvdata ( struct ib_work_queue *wq ) {
	return wq->drv_priv;
}

/**
 * Set Infiniband queue pair driver-private data
 *
 * @v qp		Queue pair
 * @v priv		Private data
 */
static inline __attribute__ (( always_inline )) void
ib_qp_set_drvdata ( struct ib_queue_pair *qp, void *priv ) {
	qp->drv_priv = priv;
}

/**
 * Get Infiniband queue pair driver-private data
 *
 * @v qp		Queue pair
 * @ret priv		Private data
 */
static inline __attribute__ (( always_inline )) void *
ib_qp_get_drvdata ( struct ib_queue_pair *qp ) {
	return qp->drv_priv;
}

/**
 * Set Infiniband queue pair owner-private data
 *
 * @v qp		Queue pair
 * @v priv		Private data
 */
static inline __attribute__ (( always_inline )) void
ib_qp_set_ownerdata ( struct ib_queue_pair *qp, void *priv ) {
	qp->owner_priv = priv;
}

/**
 * Get Infiniband queue pair owner-private data
 *
 * @v qp		Queue pair
 * @ret priv		Private data
 */
static inline __attribute__ (( always_inline )) void *
ib_qp_get_ownerdata ( struct ib_queue_pair *qp ) {
	return qp->owner_priv;
}

/**
 * Set Infiniband completion queue driver-private data
 *
 * @v cq		Completion queue
 * @v priv		Private data
 */
static inline __attribute__ (( always_inline )) void
ib_cq_set_drvdata ( struct ib_completion_queue *cq, void *priv ) {
	cq->drv_priv = priv;
}

/**
 * Get Infiniband completion queue driver-private data
 *
 * @v cq		Completion queue
 * @ret priv		Private data
 */
static inline __attribute__ (( always_inline )) void *
ib_cq_get_drvdata ( struct ib_completion_queue *cq ) {
	return cq->drv_priv;
}

/**
 * Set Infiniband device driver-private data
 *
 * @v ibdev		Infiniband device
 * @v priv		Private data
 */
static inline __attribute__ (( always_inline )) void
ib_set_drvdata ( struct ib_device *ibdev, void *priv ) {
	ibdev->drv_priv = priv;
}

/**
 * Get Infiniband device driver-private data
 *
 * @v ibdev		Infiniband device
 * @ret priv		Private data
 */
static inline __attribute__ (( always_inline )) void *
ib_get_drvdata ( struct ib_device *ibdev ) {
	return ibdev->drv_priv;
}

/**
 * Set Infiniband device owner-private data
 *
 * @v ibdev		Infiniband device
 * @v priv		Private data
 */
static inline __attribute__ (( always_inline )) void
ib_set_ownerdata ( struct ib_device *ibdev, void *priv ) {
	ibdev->owner_priv = priv;
}

/**
 * Get Infiniband device owner-private data
 *
 * @v ibdev		Infiniband device
 * @ret priv		Private data
 */
static inline __attribute__ (( always_inline )) void *
ib_get_ownerdata ( struct ib_device *ibdev ) {
	return ibdev->owner_priv;
}

/*****************************************************************************
 *
 * Management datagrams
 *
 * Portions Copyright (c) 2004 Mellanox Technologies Ltd.  All rights
 * reserved.
 *
 */

/* Management base version */
#define IB_MGMT_BASE_VERSION			1

/* Management classes */
#define IB_MGMT_CLASS_SUBN_LID_ROUTED		0x01
#define IB_MGMT_CLASS_SUBN_DIRECTED_ROUTE	0x81
#define IB_MGMT_CLASS_SUBN_ADM			0x03
#define IB_MGMT_CLASS_PERF_MGMT			0x04
#define IB_MGMT_CLASS_BM			0x05
#define IB_MGMT_CLASS_DEVICE_MGMT		0x06
#define IB_MGMT_CLASS_CM			0x07
#define IB_MGMT_CLASS_SNMP			0x08
#define IB_MGMT_CLASS_VENDOR_RANGE2_START	0x30
#define IB_MGMT_CLASS_VENDOR_RANGE2_END		0x4F

/* Management methods */
#define IB_MGMT_METHOD_GET			0x01
#define IB_MGMT_METHOD_SET			0x02
#define IB_MGMT_METHOD_GET_RESP			0x81
#define IB_MGMT_METHOD_SEND			0x03
#define IB_MGMT_METHOD_TRAP			0x05
#define IB_MGMT_METHOD_REPORT			0x06
#define IB_MGMT_METHOD_REPORT_RESP		0x86
#define IB_MGMT_METHOD_TRAP_REPRESS		0x07
#define IB_MGMT_METHOD_DELETE			0x15
#define IB_MGMT_METHOD_RESP			0x80

/* Subnet management attributes */
#define IB_SMP_ATTR_NOTICE			0x0002
#define IB_SMP_ATTR_NODE_DESC			0x0010
#define IB_SMP_ATTR_NODE_INFO			0x0011
#define IB_SMP_ATTR_SWITCH_INFO			0x0012
#define IB_SMP_ATTR_GUID_INFO			0x0014
#define IB_SMP_ATTR_PORT_INFO			0x0015
#define IB_SMP_ATTR_PKEY_TABLE			0x0016
#define IB_SMP_ATTR_SL_TO_VL_TABLE		0x0017
#define IB_SMP_ATTR_VL_ARB_TABLE		0x0018
#define IB_SMP_ATTR_LINEAR_FORWARD_TABLE	0x0019
#define IB_SMP_ATTR_RANDOM_FORWARD_TABLE	0x001A
#define IB_SMP_ATTR_MCAST_FORWARD_TABLE		0x001B
#define IB_SMP_ATTR_SM_INFO			0x0020
#define IB_SMP_ATTR_VENDOR_DIAG			0x0030
#define IB_SMP_ATTR_LED_INFO			0x0031
#define IB_SMP_ATTR_VENDOR_MASK			0xFF00

#define IB_SA_ATTR_MC_MEMBER_REC		0x38
#define IB_SA_ATTR_PATH_REC			0x35

#define IB_SA_MCMEMBER_REC_MGID			(1<<0)
#define IB_SA_MCMEMBER_REC_PORT_GID		(1<<1)
#define IB_SA_MCMEMBER_REC_QKEY			(1<<2)
#define IB_SA_MCMEMBER_REC_MLID			(1<<3)
#define IB_SA_MCMEMBER_REC_MTU_SELECTOR		(1<<4)
#define IB_SA_MCMEMBER_REC_MTU			(1<<5)
#define IB_SA_MCMEMBER_REC_TRAFFIC_CLASS	(1<<6)
#define IB_SA_MCMEMBER_REC_PKEY			(1<<7)
#define IB_SA_MCMEMBER_REC_RATE_SELECTOR	(1<<8)
#define IB_SA_MCMEMBER_REC_RATE			(1<<9)
#define IB_SA_MCMEMBER_REC_PACKET_LIFE_TIME_SELECTOR	(1<<10)
#define IB_SA_MCMEMBER_REC_PACKET_LIFE_TIME	(1<<11)
#define IB_SA_MCMEMBER_REC_SL			(1<<12)
#define IB_SA_MCMEMBER_REC_FLOW_LABEL		(1<<13)
#define IB_SA_MCMEMBER_REC_HOP_LIMIT		(1<<14)
#define IB_SA_MCMEMBER_REC_SCOPE		(1<<15)
#define IB_SA_MCMEMBER_REC_JOIN_STATE		(1<<16)
#define IB_SA_MCMEMBER_REC_PROXY_JOIN		(1<<17)

#define IB_SA_PATH_REC_DGID			(1<<2)
#define IB_SA_PATH_REC_SGID			(1<<3)

struct ib_mad_hdr {
	uint8_t base_version;
	uint8_t mgmt_class;
	uint8_t class_version;
	uint8_t method;
	uint16_t status;
	uint16_t class_specific;
	uint32_t tid[2];
	uint16_t attr_id;
	uint16_t resv;
	uint32_t attr_mod;
} __attribute__ (( packed ));

struct ib_sa_hdr {
	uint32_t sm_key[2];
	uint16_t reserved;
	uint16_t attrib_offset;
	uint32_t comp_mask[2];
} __attribute__ (( packed ));

struct ib_rmpp_hdr {
	uint32_t raw[3];
} __attribute__ (( packed ));

struct ib_mad_data {
	struct ib_mad_hdr mad_hdr;
	uint8_t data[232];
} __attribute__ (( packed ));

struct ib_mad_guid_info {
	struct ib_mad_hdr mad_hdr;
	uint32_t mkey[2];
	uint32_t reserved[8];
	uint8_t gid_local[8];
} __attribute__ (( packed ));

struct ib_mad_port_info {
	struct ib_mad_hdr mad_hdr;
	uint32_t mkey[2];
	uint32_t reserved[8];
	uint32_t mkey2[2];
	uint8_t gid_prefix[8];
	uint16_t lid;
	uint16_t mastersm_lid;
	uint32_t cap_mask;
	uint16_t diag_code;
	uint16_t mkey_lease_period;
	uint8_t local_port_num;
	uint8_t link_width_enabled;
	uint8_t link_width_supported;
	uint8_t link_width_active;
	uint8_t port_state__link_speed_supported;
	uint8_t link_down_def_state__port_phys_state;
	uint8_t lmc__r1__mkey_prot_bits;
	uint8_t link_speed_enabled__link_speed_active;
} __attribute__ (( packed ));

struct ib_mad_pkey_table {
	struct ib_mad_hdr mad_hdr;
	uint32_t mkey[2];
	uint32_t reserved[8];
	uint16_t pkey[16][2];
} __attribute__ (( packed ));

struct ib_mad_path_record {
	struct ib_mad_hdr mad_hdr;
	struct ib_rmpp_hdr rmpp_hdr;
	struct ib_sa_hdr sa_hdr;
	uint32_t reserved0[2];
	struct ib_gid dgid;
	struct ib_gid sgid;
	uint16_t dlid;
	uint16_t slid;
	uint32_t hop_limit__flow_label__raw_traffic;
	uint32_t pkey__numb_path__reversible__tclass;
	uint8_t reserved1;
	uint8_t reserved__sl;
	uint8_t mtu_selector__mtu;
	uint8_t rate_selector__rate;
	uint32_t preference__packet_lifetime__packet_lifetime_selector;
	uint32_t reserved2[35];
} __attribute__ (( packed ));

struct ib_mad_mc_member_record {
	struct ib_mad_hdr mad_hdr;
	struct ib_rmpp_hdr rmpp_hdr;
	struct ib_sa_hdr sa_hdr;
	struct ib_gid mgid;
	struct ib_gid port_gid;
	uint32_t qkey;
	uint16_t mlid;
	uint8_t mtu_selector__mtu;
	uint8_t tclass;
	uint16_t pkey;
	uint8_t rate_selector__rate;
	uint8_t packet_lifetime_selector__packet_lifetime;
	uint32_t sl__flow_label__hop_limit;
	uint8_t scope__join_state;
	uint8_t proxy_join__reserved;
	uint16_t reserved0;
	uint32_t reserved1[37];
} __attribute__ (( packed ));

union ib_mad {
	struct ib_mad_hdr mad_hdr;
	struct ib_mad_data data;
	struct ib_mad_guid_info guid_info;
	struct ib_mad_port_info port_info;
	struct ib_mad_pkey_table pkey_table;
	struct ib_mad_path_record path_record;
	struct ib_mad_mc_member_record mc_member_record;
} __attribute__ (( packed ));

#endif /* _GPXE_INFINIBAND_H */
