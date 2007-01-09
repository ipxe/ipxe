#ifndef _UNDI_H
#define _UNDI_H

/** @file
 *
 * UNDI driver
 *
 */

#include <gpxe/device.h>
#include <pxe_types.h>

/** An UNDI device
 *
 * This structure is used by assembly code as well as C; do not alter
 * this structure without editing pxeprefix.S to match.
 */
struct undi_device {
	/** PXENV+ structure address */
	SEGOFF16_t pxenv;
	/** !PXE structure address */
	SEGOFF16_t ppxe;
	/** Entry point */
	SEGOFF16_t entry;
	/** Free base memory after load */
	UINT16_t fbms;
	/** Free base memory prior to load */
	UINT16_t restore_fbms;
	/** PCI bus:dev.fn, or 0xffff */
	UINT16_t pci_busdevfn;
	/** ISAPnP card select number, or 0xffff */
	UINT16_t isapnp_csn;
	/** ISAPnP read port, or 0xffff */
	UINT16_t isapnp_read_port;
	/** Padding */
	UINT16_t pad;

	/** Generic device */
	struct device dev;
	/** Driver-private data
	 *
	 * Use undi_set_drvdata() and undi_get_drvdata() to access this
	 * field.
	 */
	void *priv;
} __attribute__ (( packed ));

/**
 * Set UNDI driver-private data
 *
 * @v undi		UNDI device
 * @v priv		Private data
 */
static inline void undi_set_drvdata ( struct undi_device *undi, void *priv ) {
	undi->priv = priv;
}

/**
 * Get UNDI driver-private data
 *
 * @v undi		UNDI device
 * @ret priv		Private data
 */
static inline void * undi_get_drvdata ( struct undi_device *undi ) {
	return undi->priv;
}

#endif /* _UNDI_H */
