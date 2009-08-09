#ifndef _GPXE_IB_CM_H
#define _GPXE_IB_CM_H

/** @file
 *
 * Infiniband communication management
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/infiniband.h>
#include <gpxe/retry.h>

struct ib_mad_transaction;
struct ib_connection;

/** Infiniband connection operations */
struct ib_connection_operations {
	/** Handle change of connection status
	 *
	 * @v ibdev		Infiniband device
	 * @v qp		Queue pair
	 * @v conn		Connection
	 * @v rc		Connection status code
	 * @v private_data	Private data, if available
	 * @v private_data_len	Length of private data
	 */
	void ( * changed ) ( struct ib_device *ibdev, struct ib_queue_pair *qp,
			     struct ib_connection *conn, int rc,
			     void *private_data, size_t private_data_len );
};

/** An Infiniband connection */
struct ib_connection {
	/** Infiniband device */
	struct ib_device *ibdev;
	/** Queue pair */
	struct ib_queue_pair *qp;
	/** Local communication ID */
	uint32_t local_id;
	/** Remote communication ID */
	uint32_t remote_id;
	/** Target service ID */
	struct ib_gid_half service_id;
	/** Connection operations */
	struct ib_connection_operations *op;

	/** List of connections */
	struct list_head list;

	/** Path to target */
	struct ib_path *path;
	/** Connection request management transaction */
	struct ib_mad_transaction *madx;

	/** Length of connection request private data */
	size_t private_data_len;
	/** Connection request private data */
	uint8_t private_data[0];
};

extern struct ib_connection *
ib_create_conn ( struct ib_device *ibdev, struct ib_queue_pair *qp,
		 struct ib_gid *dgid, struct ib_gid_half *service_id,
		 void *req_private_data, size_t req_private_data_len,
		 struct ib_connection_operations *op );
extern void ib_destroy_conn ( struct ib_device *ibdev,
			      struct ib_queue_pair *qp,
			      struct ib_connection *conn );

#endif /* _GPXE_IB_CM_H */
