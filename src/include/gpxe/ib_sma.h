#ifndef _GPXE_IB_SMA_H
#define _GPXE_IB_SMA_H

/** @file
 *
 * Infiniband Subnet Management Agent
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/infiniband.h>
#include <gpxe/ib_gma.h>

/** An Infiniband Subnet Management Agent */
struct ib_sma {
	/** General management agent */
	struct ib_gma gma;
};

extern int ib_create_sma ( struct ib_sma *sma, struct ib_device *ibdev );
extern void ib_destroy_sma ( struct ib_sma *sma );

#endif /* _GPXE_IB_SMA_H */
