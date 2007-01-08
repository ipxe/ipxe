#ifndef _UNDI_H
#define _UNDI_H

/** @file
 *
 * UNDI driver
 *
 */

#include <gpxe/device.h>
#include <pxe_types.h>

/** An UNDI device */
struct undi_device {
	/** Generic device */
	struct device dev;
	/** Driver-private data
	 *
	 * Use undi_set_drvdata() and undi_get_drvdata() to access this
	 * field.
	 */
	void *priv;

	/** PXENV+ structure address */
	SEGOFF16_t pxenv;
	/** !PXE structure address */
	SEGOFF16_t ppxe;
	/** Entry point */
	SEGOFF16_t entry;
	/** PCI bus:dev.fn, or 0 */
	unsigned int pci_busdevfn;
	/** ISAPnP card select number, or -1U */
	unsigned int isapnp_csn;
	/** ISAPnP read port, or -1U */
	unsigned int isapnp_read_port;
	/** Free base memory prior to load */
	unsigned int restore_fbms;
	/** Free base memory after load */
	unsigned int fbms;
};

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
