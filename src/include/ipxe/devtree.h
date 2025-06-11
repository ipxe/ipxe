#ifndef _IPXE_DEVTREE_H
#define _IPXE_DEVTREE_H

/** @file
 *
 * Devicetree bus
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/device.h>
#include <ipxe/dma.h>
#include <ipxe/fdt.h>

/** A devicetree device */
struct dt_device {
	/** Device name */
	const char *name;
	/** Generic device */
	struct device dev;
	/** DMA device */
	struct dma_device dma;
	/** Driver for this device */
	struct dt_driver *driver;
	/** Driver-private data */
	void *priv;
};

/** A devicetree driver */
struct dt_driver {
	/** Driver name */
	const char *name;
	/** Compatible programming model identifiers */
	const char **ids;
	/** Number of compatible programming model identifiers */
	unsigned int id_count;
	/**
	 * Probe device
	 *
	 * @v dt		Devicetree device
	 * @v offset		Starting node offset
	 * @ret rc		Return status code
	 */
	int ( * probe ) ( struct dt_device *dt, unsigned int offset );
	/**
	 * Remove device
	 *
	 * @v dt		Devicetree device
	 */
	void ( * remove ) ( struct dt_device *dt );
};

/** Devicetree driver table */
#define DT_DRIVERS __table ( struct dt_driver, "dt_drivers" )

/** Declare a devicetree driver */
#define __dt_driver __table_entry ( DT_DRIVERS, 01 )

/**
 * Set devicetree driver-private data
 *
 * @v dt		Devicetree device
 * @v priv		Private data
 */
static inline void dt_set_drvdata ( struct dt_device *dt, void *priv ) {
	dt->priv = priv;
}

/**
 * Get devicetree driver-private data
 *
 * @v dt		Devicetree device
 * @ret priv		Private data
 */
static inline void * dt_get_drvdata ( struct dt_device *dt ) {
	return dt->priv;
}

extern void * dt_ioremap ( struct dt_device *dt, unsigned int offset,
			   unsigned int index, size_t len );
extern int dt_probe_node ( struct device *parent, unsigned int offset );
extern void dt_remove_node ( struct device *parent );

#endif /* _IPXE_DEVTREE_H */
