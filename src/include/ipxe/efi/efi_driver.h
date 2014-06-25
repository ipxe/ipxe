#ifndef _IPXE_EFI_DRIVER_H
#define _IPXE_EFI_DRIVER_H

/** @file
 *
 * EFI driver interface
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <ipxe/tables.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/DevicePath.h>

/** An EFI driver */
struct efi_driver {
	/** Name */
	const char *name;
	/**
	 * Check if driver supports device
	 *
	 * @v device		Device
	 * @ret rc		Return status code
	 */
	int ( * supported ) ( EFI_HANDLE device );
	/**
	 * Attach driver to device
	 *
	 * @v device		Device
	 * @ret rc		Return status code
	 */
	int ( * start ) ( EFI_HANDLE device );
	/**
	 * Detach driver from device
	 *
	 * @v device		Device
	 */
	void ( * stop ) ( EFI_HANDLE device );
};

/** EFI driver table */
#define EFI_DRIVERS __table ( struct efi_driver, "efi_drivers" )

/** Declare an EFI driver */
#define __efi_driver( order ) __table_entry ( EFI_DRIVERS, order )

#define EFI_DRIVER_EARLY	01	/**< Early drivers */
#define EFI_DRIVER_NORMAL	02	/**< Normal drivers */
#define EFI_DRIVER_LATE		03	/**< Late drivers */

extern EFI_DEVICE_PATH_PROTOCOL *
efi_devpath_end ( EFI_DEVICE_PATH_PROTOCOL *path );
extern int efi_driver_install ( void );
extern void efi_driver_uninstall ( void );
extern int efi_driver_connect_all ( void );
extern void efi_driver_disconnect_all ( void );
extern void efi_driver_reconnect_all ( void );

#endif /* _IPXE_EFI_DRIVER_H */
