#ifndef _IPXE_PCICLOUD_H
#define _IPXE_PCICLOUD_H

/** @file
 *
 * Cloud VM PCI configuration space access
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef PCIAPI_CLOUD
#define PCIAPI_PREFIX_cloud
#else
#define PCIAPI_PREFIX_cloud __cloud_
#endif

/**
 * Check if PCI bus probing is allowed
 *
 * @ret ok		Bus probing is allowed
 */
static inline __always_inline int
PCIAPI_INLINE ( cloud, pci_can_probe ) ( void ) {
	return 1;
}

#endif /* _IPXE_PCICLOUD_H */
