/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <string.h>
#include <errno.h>
#include <endian.h>
#include <ipxe/init.h>
#include <ipxe/rotate.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_table.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_path.h>
#include <ipxe/efi/efi_cmdline.h>
#include <ipxe/efi/Protocol/LoadedImage.h>

/** Image handle passed to entry point */
EFI_HANDLE efi_image_handle;

/** Loaded image protocol for this image */
EFI_LOADED_IMAGE_PROTOCOL *efi_loaded_image;

/** Device path for the loaded image's device handle */
EFI_DEVICE_PATH_PROTOCOL *efi_loaded_image_path;

/** System table passed to entry point
 *
 * We construct the symbol name efi_systab via the PLATFORM macro.
 * This ensures that the symbol is defined only in EFI builds, and so
 * prevents EFI code from being incorrectly linked in to a non-EFI
 * build.
 */
EFI_SYSTEM_TABLE * _C2 ( PLATFORM, _systab );

/** Internal task priority level */
EFI_TPL efi_internal_tpl = TPL_CALLBACK;

/** External task priority level */
EFI_TPL efi_external_tpl = TPL_APPLICATION;

/** EFI shutdown is in progress */
int efi_shutdown_in_progress;

/** Event used to signal shutdown */
static EFI_EVENT efi_shutdown_event;

/** Stack cookie */
unsigned long __stack_chk_guard;

/** Exit function
 *
 * Cached to minimise external dependencies when a stack check
 * failure is triggered.
 */
static EFI_EXIT efi_exit;

/* Forward declarations */
static EFI_STATUS EFIAPI efi_unload ( EFI_HANDLE image_handle );

/**
 * Shut down in preparation for booting an OS.
 *
 * This hook gets called at ExitBootServices time in order to make
 * sure that everything is properly shut down before the OS takes
 * over.
 */
static EFIAPI void efi_shutdown_hook ( EFI_EVENT event __unused,
				       void *context __unused ) {

	/* This callback is invoked at TPL_NOTIFY in order to ensure
	 * that we have an opportunity to shut down cleanly before
	 * other shutdown hooks perform destructive operations such as
	 * disabling the IOMMU.
	 *
	 * Modify the internal task priority level so that no code
	 * attempts to raise from TPL_NOTIFY to TPL_CALLBACK (which
	 * would trigger a fatal exception).
	 */
	efi_internal_tpl = TPL_NOTIFY;

	/* Mark shutdown as being in progress, to indicate that large
	 * parts of the system (e.g. timers) are no longer functional.
	 */
	efi_shutdown_in_progress = 1;

	/* Shut down iPXE */
	shutdown_boot();
}

/**
 * Construct a stack cookie value
 *
 * @v handle		Image handle
 * @ret cookie		Stack cookie
 */
__attribute__ (( noinline )) unsigned long
efi_stack_cookie ( EFI_HANDLE handle ) {
	unsigned long cookie = 0;
	unsigned int rotation = ( 8 * sizeof ( cookie ) / 4 );

	/* There is no viable source of entropy available at this
	 * point.  Construct a value that is at least likely to vary
	 * between platforms and invocations.
	 */
	cookie ^= ( ( unsigned long ) handle );
	cookie = roll ( cookie, rotation );
	cookie ^= ( ( unsigned long ) &handle );
	cookie = roll ( cookie, rotation );
	cookie ^= profile_timestamp();
	cookie = roll ( cookie, rotation );
	cookie ^= build_id;

	/* Ensure that the value contains a NUL byte, to act as a
	 * runaway string terminator.  Construct the NUL using a shift
	 * rather than a mask, to avoid losing valuable entropy in the
	 * lower-order bits.
	 */
	cookie <<= 8;

	/* Ensure that the NUL byte is placed at the bottom of the
	 * stack cookie, to avoid potential disclosure via an
	 * unterminated string.
	 */
	if ( __BYTE_ORDER == __BIG_ENDIAN )
		cookie >>= 8;

	return cookie;
}

/**
 * Initialise EFI environment
 *
 * @v image_handle	Image handle
 * @v systab		System table
 * @ret efirc		EFI return status code
 */
EFI_STATUS efi_init ( EFI_HANDLE image_handle,
		      EFI_SYSTEM_TABLE *systab ) {
	EFI_BOOT_SERVICES *bs;
	struct efi_protocol *prot;
	struct efi_config_table *tab;
	EFI_DEVICE_PATH_PROTOCOL *device_path;
	void *device_path_copy;
	size_t device_path_len;
	EFI_STATUS efirc;
	int rc;

	/* Store image handle and system table pointer for future use */
	efi_image_handle = image_handle;
	efi_systab = systab;

	/* Sanity checks */
	if ( ! systab ) {
		efirc = EFI_NOT_AVAILABLE_YET;
		goto err_sanity;
	}
	if ( ! systab->ConOut ) {
		efirc = EFI_NOT_AVAILABLE_YET;
		goto err_sanity;
	}
	if ( ! systab->BootServices ) {
		DBGC ( systab, "EFI provided no BootServices entry point\n" );
		efirc = EFI_NOT_AVAILABLE_YET;
		goto err_sanity;
	}
	if ( ! systab->RuntimeServices ) {
		DBGC ( systab, "EFI provided no RuntimeServices entry "
		       "point\n" );
		efirc = EFI_NOT_AVAILABLE_YET;
		goto err_sanity;
	}
	DBGC ( systab, "EFI handle %p systab %p\n", image_handle, systab );
	bs = systab->BootServices;

	/* Store abort function pointer */
	efi_exit = bs->Exit;

	/* Look up used protocols */
	for_each_table_entry ( prot, EFI_PROTOCOLS ) {
		if ( ( efirc = bs->LocateProtocol ( &prot->guid, NULL,
						    prot->protocol ) ) == 0 ) {
			DBGC ( systab, "EFI protocol %s is at %p\n",
			       efi_guid_ntoa ( &prot->guid ),
			       *(prot->protocol) );
		} else {
			DBGC ( systab, "EFI does not provide protocol %s\n",
			       efi_guid_ntoa ( &prot->guid ) );
			/* Fail if protocol is required */
			if ( prot->required )
				goto err_missing_protocol;
		}
	}

	/* Look up used configuration tables */
	for_each_table_entry ( tab, EFI_CONFIG_TABLES ) {
		if ( ( *(tab->table) = efi_find_table ( &tab->guid ) ) ) {
			DBGC ( systab, "EFI configuration table %s is at %p\n",
			       efi_guid_ntoa ( &tab->guid ), *(tab->table) );
		} else {
			DBGC ( systab, "EFI does not provide configuration "
			       "table %s\n", efi_guid_ntoa ( &tab->guid ) );
			if ( tab->required ) {
				efirc = EFI_NOT_AVAILABLE_YET;
				goto err_missing_table;
			}
		}
	}

	/* Get loaded image protocol
	 *
	 * We assume that our loaded image protocol will not be
	 * uninstalled while our image code is still running.
	 */
	if ( ( rc = efi_open_unsafe ( image_handle,
				      &efi_loaded_image_protocol_guid,
				      &efi_loaded_image ) ) != 0 ) {
		DBGC ( systab, "EFI could not get loaded image protocol: %s",
		       strerror ( rc ) );
		efirc = EFIRC ( rc );
		goto err_no_loaded_image;
	}
	DBGC ( systab, "EFI image base address %p\n",
	       efi_loaded_image->ImageBase );

	/* Record command line */
	efi_cmdline = efi_loaded_image->LoadOptions;
	efi_cmdline_len = efi_loaded_image->LoadOptionsSize;

	/* Get loaded image's device handle's device path */
	if ( ( rc = efi_open ( efi_loaded_image->DeviceHandle,
			       &efi_device_path_protocol_guid,
			       &device_path ) ) != 0 ) {
		DBGC ( systab, "EFI could not get loaded image's device path: "
		       "%s", strerror ( rc ) );
		efirc = EFIRC ( rc );
		goto err_no_device_path;
	}

	/* Make a copy of the loaded image's device handle's device
	 * path, since the device handle itself may become invalidated
	 * when we load our own drivers.
	 */
	device_path_len = ( efi_path_len ( device_path ) +
			    sizeof ( EFI_DEVICE_PATH_PROTOCOL ) );
	if ( ( efirc = bs->AllocatePool ( EfiBootServicesData, device_path_len,
					  &device_path_copy ) ) != 0 ) {
		rc = -EEFI ( efirc );
		goto err_alloc_device_path;
	}
	memcpy ( device_path_copy, device_path, device_path_len );
	efi_loaded_image_path = device_path_copy;
	DBGC ( systab, "EFI image device path %s\n",
	       efi_devpath_text ( efi_loaded_image_path ) );

	/* EFI is perfectly capable of gracefully shutting down any
	 * loaded devices if it decides to fall back to a legacy boot.
	 * For no particularly comprehensible reason, it doesn't
	 * bother doing so when ExitBootServices() is called.
	 */
	if ( ( efirc = bs->CreateEvent ( EVT_SIGNAL_EXIT_BOOT_SERVICES,
					 TPL_NOTIFY, efi_shutdown_hook,
					 NULL, &efi_shutdown_event ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( systab, "EFI could not create ExitBootServices event: "
		       "%s\n", strerror ( rc ) );
		goto err_create_event;
	}

	/* Install driver binding protocol */
	if ( ( rc = efi_driver_install() ) != 0 ) {
		DBGC ( systab, "EFI could not install driver: %s\n",
		       strerror ( rc ) );
		efirc = EFIRC ( rc );
		goto err_driver_install;
	}

	/* Install image unload method */
	efi_loaded_image->Unload = efi_unload;

	return 0;

	efi_driver_uninstall();
 err_driver_install:
	bs->CloseEvent ( efi_shutdown_event );
 err_create_event:
	bs->FreePool ( efi_loaded_image_path );
 err_alloc_device_path:
 err_no_device_path:
 err_no_loaded_image:
 err_missing_table:
 err_missing_protocol:
 err_sanity:
	return efirc;
}

/**
 * Shut down EFI environment
 *
 * @v image_handle	Image handle
 */
static EFI_STATUS EFIAPI efi_unload ( EFI_HANDLE image_handle __unused ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_SYSTEM_TABLE *systab = efi_systab;
	struct efi_saved_tpl tpl;

	DBGC ( systab, "EFI image unloading\n" );

	/* Raise TPL */
	efi_raise_tpl ( &tpl );

	/* Shut down */
	shutdown_exit();

	/* Disconnect any remaining devices */
	efi_driver_disconnect_all();

	/* Uninstall driver binding protocol */
	efi_driver_uninstall();

	/* Uninstall exit boot services event */
	bs->CloseEvent ( efi_shutdown_event );

	/* Free copy of loaded image's device handle's device path */
	bs->FreePool ( efi_loaded_image_path );

	DBGC ( systab, "EFI image unloaded\n" );

	/* Restore TPL */
	efi_restore_tpl ( &tpl );

	return 0;
}

/**
 * Abort on stack check failure
 *
 */
__attribute__ (( noreturn )) void __stack_chk_fail ( void ) {
	EFI_STATUS efirc;
	int rc;

	/* Report failure (when debugging) */
	DBGC ( efi_systab, "EFI stack check failed (cookie %#lx); aborting\n",
	       __stack_chk_guard );

	/* Attempt to exit cleanly with an error status */
	if ( efi_exit ) {
		efirc = efi_exit ( efi_image_handle, EFI_COMPROMISED_DATA,
				   0, NULL );
		rc = -EEFI ( efirc );
		DBGC ( efi_systab, "EFI stack check exit failed: %s\n",
		       strerror ( rc ) );
	}

	/* If the exit fails for any reason, lock the system */
	while ( 1 ) {}

}

/**
 * Raise task priority level to internal level
 *
 * @v tpl		Saved TPL
 */
void efi_raise_tpl ( struct efi_saved_tpl *tpl ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	/* Record current external TPL */
	tpl->previous = efi_external_tpl;

	/* Raise TPL and record previous TPL as new external TPL */
	tpl->current = bs->RaiseTPL ( efi_internal_tpl );
	efi_external_tpl = tpl->current;
}

/**
 * Restore task priority level
 *
 * @v tpl		Saved TPL
 */
void efi_restore_tpl ( struct efi_saved_tpl *tpl ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	/* Restore external TPL */
	efi_external_tpl = tpl->previous;

	/* Restore TPL */
	bs->RestoreTPL ( tpl->current );
}
