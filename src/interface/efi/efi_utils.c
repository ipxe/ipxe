/*
 * Copyright (C) 2011 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_path.h>
#include <ipxe/efi/efi_pci.h>
#include <ipxe/efi/efi_utils.h>

/** @file
 *
 * EFI utilities
 *
 */

/**
 * Locate parent device supporting a given protocol
 *
 * @v device		EFI device handle
 * @v protocol		Protocol GUID
 * @v parent		Parent EFI device handle to fill in
 * @v skip		Number of protocol-supporting parent devices to skip
 * @ret rc		Return status code
 */
int efi_locate_device ( EFI_HANDLE device, EFI_GUID *protocol,
			EFI_HANDLE *parent, unsigned int skip ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_DEVICE_PATH_PROTOCOL *devpath;
	EFI_DEVICE_PATH_PROTOCOL *path;
	EFI_DEVICE_PATH_PROTOCOL *end;
	size_t len;
	EFI_STATUS efirc;
	int rc;

	/* Get device path */
	if ( ( rc = efi_open ( device, &efi_device_path_protocol_guid,
			       &devpath ) ) != 0 ) {
		DBGC ( device, "EFIDEV %s cannot open device path: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_open_device_path;
	}

	/* Create modifiable copy of device path */
	len = ( efi_path_len ( devpath ) + sizeof ( *end ) );
	path = malloc ( len );
	if ( ! path ) {
		rc = -ENOMEM;
		goto err_alloc_path;
	}
	memcpy ( path, devpath, len );

	/* Locate parent device(s) */
	while ( 1 ) {

		/* Check for presence of specified protocol */
		end = path;
		if ( ( efirc = bs->LocateDevicePath ( protocol, &end,
						      parent ) ) != 0 ) {
			rc = -EEFI ( efirc );
			DBGC ( device, "EFIDEV %s has no parent supporting "
			       "%s: %s\n", efi_devpath_text ( path ),
			       efi_guid_ntoa ( protocol ), strerror ( rc ) );
			goto err_locate_protocol;
		}

		/* Stop if we have skipped the requested number of devices */
		if ( ! skip-- )
			break;

		/* Trim device path */
		efi_path_terminate ( end );
		end = efi_path_prev ( path, end );
		efi_path_terminate ( end );
	}

 err_locate_protocol:
	free ( path );
 err_alloc_path:
 err_open_device_path:
	return rc;
}

/**
 * Add EFI device as child of another EFI device
 *
 * @v parent		EFI parent device handle
 * @v child		EFI child device handle
 * @ret rc		Return status code
 */
int efi_child_add ( EFI_HANDLE parent, EFI_HANDLE child ) {
	EFI_DEVICE_PATH_PROTOCOL *devpath;
	int rc;

	/* Re-open the device path protocol */
	if ( ( rc = efi_open_by_child ( parent, &efi_device_path_protocol_guid,
					child, &devpath ) ) != 0 ) {
		DBGC ( parent, "EFIDEV %s could not add child",
		       efi_handle_name ( parent ) );
		DBGC ( parent, " %s: %s\n",
		       efi_handle_name ( child ), strerror ( rc ) );
		DBGC_EFI_OPENERS ( parent, parent,
				   &efi_device_path_protocol_guid );
		return rc;
	}

	DBGC2 ( parent, "EFIDEV %s added child", efi_handle_name ( parent ) );
	DBGC2 ( parent, " %s\n", efi_handle_name ( child ) );
	return 0;
}

/**
 * Remove EFI device as child of another EFI device
 *
 * @v parent		EFI parent device handle
 * @v child		EFI child device handle
 */
void efi_child_del ( EFI_HANDLE parent, EFI_HANDLE child ) {

	efi_close_by_child ( parent, &efi_device_path_protocol_guid, child );
	DBGC2 ( parent, "EFIDEV %s removed child", efi_handle_name ( parent ) );
	DBGC2 ( parent, " %s\n", efi_handle_name ( child ) );
}

/**
 * Get underlying PCI device information
 *
 * @v device		EFI device handle
 * @v prefix		Device name prefix
 * @v dev		Generic device to fill in
 * @ret rc		Return status code
 */
static int efi_device_info_pci ( EFI_HANDLE device, const char *prefix,
				 struct device *dev ) {
	EFI_HANDLE pci_device;
	struct efi_pci_device efipci;
	int rc;

	/* Find parent PCI device */
	if ( ( rc = efi_locate_device ( device, &efi_pci_io_protocol_guid,
					&pci_device, 0 ) ) != 0 ) {
		DBGC ( device, "EFIDEV %s is not a PCI device: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}

	/* Get PCI device information */
	if ( ( rc = efipci_info ( pci_device, &efipci ) ) != 0 ) {
		DBGC ( device, "EFIDEV %s could not get PCI information: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}

	/* Populate device information */
	memcpy ( &dev->desc, &efipci.pci.dev.desc, sizeof ( dev->desc ) );
	snprintf ( dev->name, sizeof ( dev->name ), "%s-%s",
		   prefix, efipci.pci.dev.name );

	return 0;
}

/**
 * Get underlying device information
 *
 * @v device		EFI device handle
 * @v prefix		Device name prefix
 * @v dev		Generic device to fill in
 */
void efi_device_info ( EFI_HANDLE device, const char *prefix,
		       struct device *dev ) {
	int rc;

	/* Try getting underlying PCI device information */
	if ( ( rc = efi_device_info_pci ( device, prefix, dev ) ) == 0 )
		return;

	/* If we cannot get any underlying device information, fall
	 * back to providing information about the EFI handle.
	 */
	DBGC ( device, "EFIDEV %s could not get underlying device "
	       "information\n", efi_handle_name ( device ) );
	dev->desc.bus_type = BUS_TYPE_EFI;
	snprintf ( dev->name, sizeof ( dev->name ), "%s-%p", prefix, device );
}
