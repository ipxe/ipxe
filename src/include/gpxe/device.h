#ifndef _GPXE_DEVICE_H
#define _GPXE_DEVICE_H

/**
 * @file
 *
 * Device model
 *
 */

#include <gpxe/list.h>
#include <gpxe/tables.h>

/** A PCI device description */
struct pci_device_description {
	/** Bus type
	 *
	 * Must be @c BUS_TYPE_PCI.
	 */
	unsigned int bus_type;
	/** Bus:dev.fn address
	 *
	 * As constructed by PCI_BUSDEVFN().
	 */
	unsigned int busdevfn;
	/** Vendor ID */
	unsigned int vendor;
	/** Device ID */
	unsigned int device;
};

/** PCI bus type */
#define BUS_TYPE_PCI 1

/** An ISAPnP device description */
struct isapnp_device_description {
	/** Bus type
	 *
	 * Must be @c BUS_TYPE_ISAPNP.
	 */
	unsigned int bus_type;
};

/** PCI bus type */
#define BUS_TYPE_ISAPNP 2

/** A hardware device description */
union device_description {
	/** Bus type
	 *
	 * This must be a BUS_TYPE_XXX constant.
	 */
	unsigned int bus_type;
	/** PCI device description */
	struct pci_device_description pci;
	/** ISAPnP device description */
	struct isapnp_device_description isapnp;
};

/** A hardware device */
struct device {
	/** Name */
	char name[16];
	/** Device description */
	union device_description desc;
	/** Devices on the same bus */
	struct list_head siblings;
	/** Devices attached to this device */
	struct list_head children;
	/** Bus device */
	struct device *parent;
};

/**
 * A root device
 *
 * Root devices are system buses such as PCI, EISA, etc.
 *
 */
struct root_device {
	/** Device chain
	 *
	 * A root device has a NULL parent field.
	 */
	struct device dev;
	/** Root device driver */
	struct root_driver *driver;
};

/** A root device driver */
struct root_driver {
	/**
	 * Add root device
	 *
	 * @v rootdev	Root device
	 * @ret rc	Return status code
	 *
	 * Called from probe_devices() for all root devices in the build.
	 */
	int ( * probe ) ( struct root_device *rootdev );
	/**
	 * Remove root device
	 *
	 * @v rootdev	Root device
	 *
	 * Called from remove_device() for all successfully-probed
	 * root devices.
	 */
	void ( * remove ) ( struct root_device *rootdev );
};

/** Declare a root device */
#define __root_device __table ( struct root_device, root_devices, 01 )

extern int probe_devices ( void );
extern void remove_devices ( void );

#endif /* _GPXE_DEVICE_H */
