#ifndef _IPXE_EFI_DRIVER_H
#define _IPXE_EFI_DRIVER_H

/** @file
 *
 * EFI driver interface
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/device.h>
#include <ipxe/tables.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/DevicePath.h>

/** An EFI device */
struct efi_device {
	/** Generic device */
	struct device dev;
	/** EFI device handle */
	EFI_HANDLE device;
	/** EFI child device handle (if present) */
	EFI_HANDLE child;
	/** EFI device path copy */
	EFI_DEVICE_PATH_PROTOCOL *path;
	/** Driver for this device */
	struct efi_driver *driver;
	/** Driver-private data */
	void *priv;
};

/** An EFI driver */
struct efi_driver {
	/** Name */
	const char *name;
	/**
	 * Exclude existing drivers
	 *
	 * @v device		EFI device handle
	 * @ret rc		Return status code
	 */
	int ( * exclude ) ( EFI_HANDLE device );
	/**
	 * Check if driver supports device
	 *
	 * @v device		EFI device handle
	 * @ret rc		Return status code
	 */
	int ( * supported ) ( EFI_HANDLE device );
	/**
	 * Attach driver to device
	 *
	 * @v efidev		EFI device
	 * @ret rc		Return status code
	 */
	int ( * start ) ( struct efi_device *efidev );
	/**
	 * Detach driver from device
	 *
	 * @v efidev		EFI device
	 */
	void ( * stop ) ( struct efi_device *efidev );
};

/** EFI driver table */
#define EFI_DRIVERS __table ( struct efi_driver, "efi_drivers" )

/** Declare an EFI driver */
#define __efi_driver( order ) __table_entry ( EFI_DRIVERS, order )

#define EFI_DRIVER_EARLY	01	/**< Early drivers */
#define EFI_DRIVER_HARDWARE	02	/**< Hardware drivers */
#define EFI_DRIVER_NII		03	/**< NII protocol drivers */
#define EFI_DRIVER_SNP		04	/**< SNP protocol drivers */
#define EFI_DRIVER_MNP		05	/**< MNP protocol drivers */

/**
 * Set EFI driver-private data
 *
 * @v efidev		EFI device
 * @v priv		Private data
 */
static inline void efidev_set_drvdata ( struct efi_device *efidev,
					void *priv ) {
	efidev->priv = priv;
}

/**
 * Get EFI driver-private data
 *
 * @v efidev		EFI device
 * @ret priv		Private data
 */
static inline void * efidev_get_drvdata ( struct efi_device *efidev ) {
	return efidev->priv;
}

extern struct efi_device * efidev_alloc ( EFI_HANDLE device );
extern void efidev_free ( struct efi_device *efidev );
extern struct efi_device * efidev_parent ( struct device *dev );
extern int efi_driver_install ( void );
extern void efi_driver_uninstall ( void );
extern int efi_driver_exclude ( EFI_HANDLE device, EFI_GUID *protocol );
extern int efi_driver_connect_all ( void );
extern void efi_driver_disconnect_all ( void );
extern void efi_driver_reconnect_all ( void );

#endif /* _IPXE_EFI_DRIVER_H */
