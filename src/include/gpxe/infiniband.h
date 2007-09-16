#ifndef _GPXE_INFINIBAND_H
#define _GPXE_INFINIBAND_H

/** @file
 *
 * Infiniband protocol
 *
 */

#include <stdint.h>
#include <gpxe/netdevice.h>

/** An Infiniband Global Identifier */
struct ib_gid {
	uint8_t bytes[16];
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

/** Infiniband MAC address length */
#define IB_ALEN 20

/** An Infiniband MAC address */
struct ib_mac {
	/** Queue pair number
	 *
	 * MSB must be zero; QPNs are only 24-bit.
	 */
	uint32_t qpn;
	/** Port GID */
	struct ib_gid gid;
} __attribute__ (( packed ));

/** Infiniband link-layer header length */
#define IB_HLEN 4

/** An Infiniband link-layer header */
struct ibhdr {
	/** Network-layer protocol */
	uint16_t proto;
	/** Reserved, must be zero */
	uint16_t reserved;
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
	/** Device private data */
	void *dev_priv;
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
	/** Device private data */
	void *dev_priv;
	/** Queue owner private data */
	void *owner_priv;
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
	/** Device private data */
	void *dev_priv;
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
	unsigned int qkey;
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
	/**
	 * Post receive work queue entry
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
};

/** An Infiniband device */
struct ib_device {	
	/** Infiniband operations */
	struct ib_device_operations *op;
	/** Device private data */
	void *dev_priv;
};

extern struct ib_completion_queue * ib_create_cq ( struct ib_device *ibdev,
						   unsigned int num_cqes );
extern void ib_destroy_cq ( struct ib_device *ibdev,
			    struct ib_completion_queue *cq );
extern struct ib_queue_pair *
ib_create_qp ( struct ib_device *ibdev, unsigned int num_send_wqes,
	       struct ib_completion_queue *send_cq, unsigned int num_recv_wqes,
	       struct ib_completion_queue *recv_cq, unsigned long qkey );
extern void ib_destroy_qp ( struct ib_device *ibdev,
			    struct ib_queue_pair *qp );
extern struct ib_work_queue * ib_find_wq ( struct ib_completion_queue *cq,
					   unsigned long qpn, int is_send );



extern struct ll_protocol infiniband_protocol;

extern const char * ib_ntoa ( const void *ll_addr );

/**
 * Allocate Infiniband device
 *
 * @v priv_size		Size of driver private data
 * @ret netdev		Network device, or NULL
 */
static inline struct net_device * alloc_ibdev ( size_t priv_size ) {
	struct net_device *netdev;

	netdev = alloc_netdev ( priv_size );
	if ( netdev ) {
		netdev->ll_protocol = &infiniband_protocol;
	}
	return netdev;
}

#endif /* _GPXE_INFINIBAND_H */
