#ifndef _GPXE_IB_SMC_H
#define _GPXE_IB_SMC_H

/** @file
 *
 * Infiniband Subnet Management Client
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/infiniband.h>

typedef int ( * ib_local_mad_t ) ( struct ib_device *ibdev,
				   union ib_mad *mad );

extern int ib_smc_update ( struct ib_device *ibdev,
			   ib_local_mad_t local_mad );

#endif /* _GPXE_IB_SMC_H */
