#ifndef _IPXE_IB_SRP_H
#define _IPXE_IB_SRP_H

/** @file
 *
 * SCSI RDMA Protocol over Infiniband
 *
 */

FILE_LICENCE ( BSD2 );

#include <stdint.h>
#include <ipxe/acpi.h>
#include <ipxe/interface.h>
#include <ipxe/infiniband.h>
#include <ipxe/srp.h>

/** SRP initiator port identifier for Infiniband */
union ib_srp_initiator_port_id {
	/** SRP version of port identifier */
	union srp_port_id srp;
	/** Infiniband version of port identifier */
	struct {
		/** Identifier extension */
		union ib_guid id_ext;
		/** IB channel adapter GUID */
		union ib_guid hca_guid;
	} __attribute__ (( packed )) ib;
};

/** SRP target port identifier for Infiniband */
union ib_srp_target_port_id {
	/** SRP version of port identifier */
	union srp_port_id srp;
	/** Infiniband version of port identifier */
	struct {
		/** Identifier extension */
		union ib_guid id_ext;
		/** I/O controller GUID */
		union ib_guid ioc_guid;
	} __attribute__ (( packed )) ib;
};

/**
 * sBFT Infiniband subtable
 */
struct sbft_ib_subtable {
	/** Source GID */
	union ib_gid sgid;
	/** Destination GID */
	union ib_gid dgid;
	/** Service ID */
	union ib_guid service_id;
	/** Partition key */
	uint16_t pkey;
	/** Reserved */
	uint8_t reserved[6];
} __attribute__ (( packed ));

/**
 * An Infiniband SRP sBFT created by iPXE
 */
struct ipxe_ib_sbft {
	/** The table header */
	struct sbft_table table;
	/** The SCSI subtable */
	struct sbft_scsi_subtable scsi;
	/** The SRP subtable */
	struct sbft_srp_subtable srp;
	/** The Infiniband subtable */
	struct sbft_ib_subtable ib;
};

/** An Infiniband SRP device */
struct ib_srp_device {
	/** Reference count */
	struct refcnt refcnt;

	/** SRP transport interface */
	struct interface srp;
	/** CMRC interface */
	struct interface cmrc;

	/** Infiniband device */
	struct ib_device *ibdev;

	/** ACPI descriptor */
	struct acpi_descriptor desc;
	/** Boot firmware table parameters */
	struct ipxe_ib_sbft sbft;
};

#endif /* _IPXE_IB_SRP_H */
