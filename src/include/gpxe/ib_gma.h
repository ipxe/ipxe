#ifndef _GPXE_IB_GMA_H
#define _GPXE_IB_GMA_H

/** @file
 *
 * Infiniband General Management Agent
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/list.h>
#include <gpxe/retry.h>
#include <gpxe/tables.h>

struct ib_device;
struct ib_completion_queue;
struct ib_queue_pair;
union ib_mad;

/** A MAD attribute handler */
struct ib_mad_handler {
	/** Management class */
	uint8_t mgmt_class;
	/** Class version */
	uint8_t class_version;
	/** Method */
	uint8_t method;
	/** Response method, or zero */
	uint8_t resp_method;
	/** Attribute (in network byte order) */
	uint16_t attr_id;
	/** Handle attribute
	 *
	 * @v ibdev	Infiniband device
	 * @v mad	MAD
	 * @ret rc	Return status code
	 *
	 * The handler should modify the MAD as applicable.  If the
	 * handler returns with a non-zero value in the MAD's @c
	 * method field, it will be sent as a response.
	 */
	int ( * handle ) ( struct ib_device *ibdev, union ib_mad *mad );
};

/** MAD attribute handlers */
#define IB_MAD_HANDLERS __table ( struct ib_mad_handler, "ib_mad_handlers" )

/** Declare a MAD attribute handler */
#define __ib_mad_handler __table_entry ( IB_MAD_HANDLERS, 01 )

/** An Infiniband General Management Agent */
struct ib_gma {
	/** Infiniband device */
	struct ib_device *ibdev;
	/** Completion queue */
	struct ib_completion_queue *cq;
	/** Queue pair */
	struct ib_queue_pair *qp;

	/** List of outstanding MAD requests */
	struct list_head requests;
};

extern int ib_gma_request ( struct ib_gma *gma, union ib_mad *mad,
			    struct ib_address_vector *av );
extern int ib_create_gma ( struct ib_gma *gma, struct ib_device *ibdev,
			   unsigned long qkey );
extern void ib_destroy_gma ( struct ib_gma *gma );

#endif /* _GPXE_IB_GMA_H */
