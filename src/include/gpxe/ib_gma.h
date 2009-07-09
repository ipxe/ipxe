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
#include <gpxe/infiniband.h>

struct ib_gma;

/** A GMA attribute handler */
struct ib_gma_handler {
	/** Management class */
	uint8_t mgmt_class;
	/** Management class don't-care bits */
	uint8_t mgmt_class_ignore;
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
	 * @v gma	General management agent
	 * @v mad	MAD
	 * @ret rc	Return status code
	 *
	 * The handler should modify the MAD as applicable.  If the
	 * handler returns with a non-zero value in the MAD's @c
	 * method field, it will be sent as a response.
	 */
	int ( * handle ) ( struct ib_gma *gma, union ib_mad *mad );
};

/** GMA attribute handlers */
#define IB_GMA_HANDLERS __table ( struct ib_gma_handler, "ib_gma_handlers" )

/** Declare a GMA attribute handler */
#define __ib_gma_handler __table_entry ( IB_GMA_HANDLERS, 01 )

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
			    struct ib_address_vector *av, int retry );
extern struct ib_gma * ib_create_gma ( struct ib_device *ibdev,
				       enum ib_queue_pair_type type );
extern void ib_destroy_gma ( struct ib_gma *gma );

#endif /* _GPXE_IB_GMA_H */
