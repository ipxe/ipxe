#ifndef _GPXE_IB_CM_H
#define _GPXE_IB_CM_H

/** @file
 *
 * Infiniband communication management
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/infiniband.h>

extern int ib_cm_connect ( struct ib_queue_pair *qp, struct ib_gid *dgid,
			   struct ib_gid_half *service_id,
			   void *private_data, size_t private_data_len,
			   void ( * notify ) ( struct ib_queue_pair *qp,
					       int rc, void *private_data,
					       size_t private_data_len ) );

#endif /* _GPXE_IB_CM_H */
