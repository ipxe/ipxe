#ifndef _GPXE_IB_SMA_H
#define _GPXE_IB_SMA_H

/** @file
 *
 * Infiniband Subnet Management Agent
 *
 */

#include <gpxe/infiniband.h>
#include <gpxe/process.h>

/** Infiniband Subnet Management Agent operations */
struct ib_sma_operations {
	/** Set port information
	 *
	 * @v ibdev		Infiniband device
	 * @v port_info		New port information
	 */
	int ( * set_port_info ) ( struct ib_device *ibdev,
				  const struct ib_port_info *port_info );
};

/** An Infiniband Subnet Management Agent */
struct ib_sma {
	/** Infiniband device */
	struct ib_device *ibdev;
	/** SMA operations */
	struct ib_sma_operations *op;
	/** SMA completion queue */
	struct ib_completion_queue *cq;
	/** SMA queue pair */
	struct ib_queue_pair *qp;
	/** Poll process */
	struct process poll;
};

/** SMA payload size allocated for received packets */
#define IB_SMA_PAYLOAD_LEN 2048

/** SMA number of send WQEs
 *
 * This is a policy decision.
 */
#define IB_SMA_NUM_SEND_WQES 4

/** SMA number of receive WQEs
 *
 * This is a policy decision.
 */
#define IB_SMA_NUM_RECV_WQES 2

/** SMA number of completion queue entries
 *
 * This is a policy decision
 */
#define IB_SMA_NUM_CQES 8

extern int ib_create_sma ( struct ib_sma *sma, struct ib_device *ibdev,
			   struct ib_sma_operations *op );
extern void ib_destroy_sma ( struct ib_sma *sma );

#endif /* _GPXE_IB_SMA_H */
