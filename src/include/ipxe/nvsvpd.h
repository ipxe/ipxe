#ifndef _IPXE_NVSVPD_H
#define _IPXE_NVSVPD_H

/**
 * @file
 *
 * Non-Volatile Storage using Vital Product Data
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <ipxe/nvs.h>
#include <ipxe/pcivpd.h>

/** An NVS VPD device */
struct nvs_vpd_device {
	/** NVS device */
	struct nvs_device nvs;
	/** PCI VPD device */
	struct pci_vpd vpd;
	/** Starting address
	 *
	 * This address is added to the NVS address to form the VPD
	 * address.
	 */
	unsigned int address;
};

extern int nvs_vpd_init ( struct nvs_vpd_device *nvsvpd, struct pci_device *pci,
			  unsigned int field );

#endif /* IPXE_NVSVPD_H */
