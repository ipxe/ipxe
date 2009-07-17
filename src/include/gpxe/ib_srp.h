#ifndef _GPXE_IB_SRP_H
#define _GPXE_IB_SRP_H

/** @file
 *
 * SCSI RDMA Protocol over Infiniband
 *
 */

FILE_LICENCE ( BSD2 );

#include <stdint.h>
#include <gpxe/infiniband.h>
#include <gpxe/srp.h>

/** SRP initiator port identifier for Infiniband */
struct ib_srp_initiator_port_id {
	/** Identifier extension */
	struct ib_gid_half id_ext;
	/** IB channel adapter GUID */
	struct ib_gid_half hca_guid;
} __attribute__ (( packed ));

/** SRP target port identifier for Infiniband */
struct ib_srp_target_port_id {
	/** Identifier extension */
	struct ib_gid_half id_ext;
	/** I/O controller GUID */
	struct ib_gid_half ioc_guid;
} __attribute__ (( packed ));

/**
 * Get Infiniband-specific initiator port ID
 *
 * @v port_ids		SRP port IDs
 * @ret initiator_port_id  Infiniband-specific initiator port ID
 */
static inline __always_inline struct ib_srp_initiator_port_id *
ib_srp_initiator_port_id ( struct srp_port_ids *port_ids ) {
	return ( ( struct ib_srp_initiator_port_id * ) &port_ids->initiator );
}

/**
 * Get Infiniband-specific target port ID
 *
 * @v port_ids		SRP port IDs
 * @ret target_port_id	Infiniband-specific target port ID
 */
static inline __always_inline struct ib_srp_target_port_id *
ib_srp_target_port_id ( struct srp_port_ids *port_ids ) {
	return ( ( struct ib_srp_target_port_id * ) &port_ids->target );
}

/** Infiniband-specific SRP parameters */
struct ib_srp_parameters {
	/** Source GID */
	struct ib_gid sgid;
	/** Destination GID */
	struct ib_gid dgid;
	/** Service ID */
	struct ib_gid_half service_id;
	/** Partition key */
	uint16_t pkey;
};

/**
 * Get Infiniband-specific transport parameters
 *
 * @v srp		SRP device
 * @ret ib_params	Infiniband-specific transport parameters
 */
static inline __always_inline struct ib_srp_parameters *
ib_srp_params ( struct srp_device *srp ) {
	return srp_transport_priv ( srp );
}

extern struct srp_transport_type ib_srp_transport;

#endif /* _GPXE_IB_SRP_H */
