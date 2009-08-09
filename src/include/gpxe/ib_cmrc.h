#ifndef _GPXE_IB_CMRC_H
#define _GPXE_IB_CMRC_H

/** @file
 *
 * Infiniband Communication-managed Reliable Connections
 *
 */

FILE_LICENCE ( BSD2 );

#include <gpxe/infiniband.h>
#include <gpxe/xfer.h>

extern int ib_cmrc_open ( struct xfer_interface *xfer,
			  struct ib_device *ibdev,
			  struct ib_gid *dgid,
			  struct ib_gid_half *service_id );

#endif /* _GPXE_IB_CMRC_H */
