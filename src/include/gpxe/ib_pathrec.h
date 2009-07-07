#ifndef _GPXE_IB_PATHREC_H
#define _GPXE_IB_PATHREC_H

/** @file
 *
 * Infiniband path records
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/infiniband.h>

extern int ib_resolve_path ( struct ib_device *ibdev,
			     struct ib_address_vector *av );

#endif /* _GPXE_IB_PATHREC_H */
