/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <string.h>
#include <errno.h>
#include <ipxe/timer.h>
#include <ipxe/image.h>
#include <ipxe/netdevice.h>
#include <ipxe/uri.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_utils.h>
#include <ipxe/efi/efi_autoexec.h>
#include <ipxe/efi/mnpnet.h>
#include <usr/imgmgmt.h>
#include <usr/sync.h>

/** @file
 *
 * EFI autoexec script
 *
 */

/** Timeout for autoexec script downloads */
#define EFI_AUTOEXEC_TIMEOUT ( 30 * TICKS_PER_SEC )

/** Timeout for autoexec pending operation completion */
#define EFI_AUTOEXEC_SYNC_TIMEOUT ( 1 * TICKS_PER_SEC )

/** Autoexec script image name */
#define EFI_AUTOEXEC_NAME "autoexec.ipxe"

/** An EFI autoexec script loader */
struct efi_autoexec_loader {
	/** Required protocol GUID */
	EFI_GUID *protocol;
	/**
	 * Load autoexec script
	 *
	 * @v handle		Handle on which protocol was found
	 * @v image		Image to fill in
	 * @ret rc		Return status code
	 */
	int ( * load ) ( EFI_HANDLE handle, struct image **image );
};

/**
 * Load autoexec script from filesystem
 *
 * @v handle		Simple filesystem protocol handle
 * @v image		Image to fill in
 * @ret rc		Return status code
 */
static int efi_autoexec_filesystem ( EFI_HANDLE handle, struct image **image ) {
	EFI_HANDLE device = efi_loaded_image->DeviceHandle;
	int rc;

	/* Check that we were loaded from a filesystem */
	if ( handle != device ) {
		DBGC ( device, "EFI %s is not the file system handle\n",
		       efi_handle_name ( device ) );
		return -ENOTTY;
	}

	/* Try loading from loaded image directory, if supported */
	if ( ( rc = imgacquire ( "file:" EFI_AUTOEXEC_NAME,
				 EFI_AUTOEXEC_TIMEOUT, image ) ) == 0 )
		return 0;

	/* Try loading from root directory, if supported */
	if ( ( rc = imgacquire ( "file:/" EFI_AUTOEXEC_NAME,
				 EFI_AUTOEXEC_TIMEOUT, image ) ) == 0 )
		return 0;

	return rc;
}

/**
 * Load autoexec script via temporary network device
 *
 * @v handle		Managed network protocol service binding handle
 * @v image		Image to fill in
 * @ret rc		Return status code
 */
static int efi_autoexec_network ( EFI_HANDLE handle, struct image **image ) {
	EFI_HANDLE device = efi_loaded_image->DeviceHandle;
	struct net_device *netdev;
	int rc;

	/* Create temporary network device */
	if ( ( rc = mnptemp_create ( handle, &netdev ) ) != 0 ) {
		DBGC ( device, "EFI %s could not create net device: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_create;
	}

	/* Do nothing unless we have a usable current working URI */
	if ( ! cwuri ) {
		DBGC ( device, "EFI %s has no current working URI\n",
		       efi_handle_name ( device ) );
		rc = -ENOTTY;
		goto err_cwuri;
	}

	/* Open network device */
	if ( ( rc = netdev_open ( netdev ) ) != 0 ) {
		DBGC ( device, "EFI %s could not open net device: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_open;
	}

	/* Attempt download */
	rc = imgacquire ( EFI_AUTOEXEC_NAME, EFI_AUTOEXEC_TIMEOUT, image );
	if ( rc != 0 ) {
		DBGC ( device, "EFI %s could not download %s: %s\n",
		       efi_handle_name ( device ), EFI_AUTOEXEC_NAME,
		       strerror ( rc ) );
	}

	/* Ensure network exchanges have completed */
	sync ( EFI_AUTOEXEC_SYNC_TIMEOUT );

 err_open:
 err_cwuri:
	mnptemp_destroy ( netdev );
 err_create:
	return rc;
}

/** Autoexec script loaders */
static struct efi_autoexec_loader efi_autoexec_loaders[] = {
	{
		.protocol = &efi_simple_file_system_protocol_guid,
		.load = efi_autoexec_filesystem,
	},
	{
		.protocol = &efi_managed_network_service_binding_protocol_guid,
		.load = efi_autoexec_network,
	},
};

/**
 * Load autoexec script
 *
 * @ret rc		Return status code
 */
int efi_autoexec_load ( void ) {
	EFI_HANDLE device = efi_loaded_image->DeviceHandle;
	EFI_HANDLE handle;
	struct efi_autoexec_loader *loader;
	struct image *image;
	unsigned int i;
	int rc;

	/* Use first applicable loader */
	for ( i = 0 ; i < ( sizeof ( efi_autoexec_loaders ) /
			    sizeof ( efi_autoexec_loaders[0] ) ) ; i ++ ) {

		/* Locate required protocol for this loader */
		loader = &efi_autoexec_loaders[i];
		if ( ( rc = efi_locate_device ( device, loader->protocol,
						&handle, 0 ) ) != 0 ) {
			DBGC ( device, "EFI %s found no %s: %s\n",
			       efi_handle_name ( device ),
			       efi_guid_ntoa ( loader->protocol ),
			       strerror ( rc ) );
			continue;
		}
		DBGC ( device, "EFI %s found %s on ",
		       efi_handle_name ( device ),
		       efi_guid_ntoa ( loader->protocol ) );
		DBGC ( device, "%s\n", efi_handle_name ( handle ) );

		/* Try loading */
		if ( ( rc = loader->load ( handle, &image ) ) != 0 )
			return rc;

		/* Discard zero-length images */
		if ( ! image->len ) {
			DBGC ( device, "EFI %s discarding zero-length %s\n",
			       efi_handle_name ( device ), image->name );
			unregister_image ( image );
			return -ENOENT;
		}

		DBGC ( device, "EFI %s loaded %s (%zd bytes)\n",
		       efi_handle_name ( device ), image->name, image->len );
		return 0;
	}

	return -ENOENT;
}
