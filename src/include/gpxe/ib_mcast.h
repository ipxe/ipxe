#ifndef _GPXE_IB_MCAST_H
#define _GPXE_IB_MCAST_H

/** @file
 *
 * Infiniband multicast groups
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/infiniband.h>

extern int ib_mcast_join ( struct ib_device *ibdev, struct ib_queue_pair *qp,
			   struct ib_gid *gid );
extern void ib_mcast_leave ( struct ib_device *ibdev, struct ib_queue_pair *qp,
			     struct ib_gid *gid );

#endif /* _GPXE_IB_MCAST_H */
