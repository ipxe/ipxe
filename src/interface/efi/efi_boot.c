/*
 * Copyright (C) 2019 Oracle. All rights reserved.
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

/**
 * @file
 *
 * EFI boot local protocols
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ipxe/refcnt.h>
#include <ipxe/list.h>
#include <ipxe/uri.h>
#include <ipxe/interface.h>
#include <ipxe/blockdev.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/retry.h>
#include <ipxe/timer.h>
#include <ipxe/process.h>
#include <ipxe/sanboot.h>
#include <ipxe/iso9660.h>
#include <ipxe/acpi.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_path.h>
#include <ipxe/efi/efi_strings.h>
#include <ipxe/efi/efi_snp.h>
#include <ipxe/efi/efi_utils.h>
#include <ipxe/efi/efi_block.h>
#include <ipxe/efi/Guid/FileInfo.h>
#include <ipxe/efi/Guid/FileSystemInfo.h>
#include <ipxe/efi/Protocol/BlockIo.h>
#include <ipxe/efi/Protocol/SimpleFileSystem.h>
#include <ipxe/efi/Protocol/AcpiTable.h>

static wchar_t efi_default_boot_filename[] = EFI_REMOVABLE_MEDIA_FILE_NAME;

static EFI_DEVICE_PATH_PROTOCOL	**DevicePathList;
static UINTN			  DevicePathListNum;
static EFI_HANDLE		 *SimpleFSHandleList;
static BOOLEAN			  efi_boot_map_initialized = FALSE;

static EFI_HANDLE * efi_boot_get_handlelist ( EFI_GUID *ProtocolGuid ) {
	EFI_BOOT_SERVICES	*bs = efi_systab->BootServices;
	EFI_HANDLE		*HandleList;
	UINTN			 Size;
	EFI_STATUS		 efirc;
	EFI_LOCATE_SEARCH_TYPE	 SearchType;

	/* NULL ProtocolGuid gets all handles in system */
	if ( ProtocolGuid )
		SearchType = ByProtocol;
	else
		SearchType = AllHandles;

	Size = 0;
	HandleList = NULL;

	/* First call gets the handle list size and returns BUFFER_TOO_SMALL. */
	efirc = bs->LocateHandle ( SearchType, (EFI_GUID*) ProtocolGuid,
				 NULL, &Size, HandleList );
	if ( efirc == EFI_BUFFER_TOO_SMALL ) {
		/* Alloc an extra handle for NULL list terminator */
		if ( ( efirc = bs->AllocatePool ( EfiBootServicesData,
					          ( Size +
						    sizeof ( EFI_HANDLE ) ),
					          (void **) &HandleList ) )
					          != 0 ) {
			return ( NULL ) ;
		}

		efirc = bs->LocateHandle ( SearchType, (EFI_GUID*) ProtocolGuid,
					 NULL, &Size, HandleList );
		if ( HandleList )
			HandleList[Size / sizeof ( EFI_HANDLE )] = NULL;
	}

	if ( EFI_ERROR ( efirc ) ) {
		if ( HandleList )
			bs->FreePool ( HandleList );
		return ( NULL ) ;
	}

	return ( HandleList );
}

static EFI_DEVICE_PATH_PROTOCOL * efi_boot_get_devpath ( EFI_HANDLE Handle ) {
	EFI_BOOT_SERVICES		*bs = efi_systab->BootServices;
	EFI_DEVICE_PATH_PROTOCOL	*DevicePath;
	EFI_STATUS			 efirc;

	efirc = bs->HandleProtocol ( Handle, &efi_device_path_protocol_guid,
				    (VOID *)  &DevicePath );

	if ( EFI_ERROR ( efirc ) )
		return NULL;
	else
		return DevicePath;
}

static void efi_boot_connect_pcibridges ( void ) {
	EFI_BOOT_SERVICES	*bs = efi_systab->BootServices;
	EFI_HANDLE		*HandleList;
	UINTN			 Count;

	HandleList = efi_boot_get_handlelist ( &efi_pci_root_bridge_io_protocol_guid );
	if ( HandleList == NULL ) {
		DBG ( "EFIBOOT efi_boot_connect_pcibridges: no handles!\n" );
		return;
	}

	for ( Count = 0 ; HandleList[Count] != NULL ; Count++ ) {

		DBG ( "EFIBOOT efi_boot_connect_pcibridges: connecting "
		      "handle %s\n", efi_handle_name ( HandleList[Count] ) );

		 (void)  bs->ConnectController ( HandleList[Count], NULL,
						 NULL, 1 );

		DBG ( "EFIBOOT: handle %s supports protocols:\n",
		      efi_handle_name ( HandleList[Count] ) );
		DBG_EFI_PROTOCOLS_IF ( LOG, HandleList[Count] );
	}

	bs->FreePool ( HandleList );

	return;
}

static int efi_vol_label( EFI_HANDLE handle, char *label_buf,
			     size_t label_buf_size ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_FILE_SYSTEM_INFO *info;
	EFI_FILE_PROTOCOL *root;
	EFI_STATUS efirc;
	UINTN size;
	int rc;
	union {
		void *interface;
		EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
	} u;

	/* Open file system protocol */
	if ( ( efirc = bs->OpenProtocol ( handle,
					  &efi_simple_file_system_protocol_guid,
					  &u.interface, efi_image_handle,
					  handle,
					  EFI_OPEN_PROTOCOL_GET_PROTOCOL ))!=0){
		rc = -1;
		DBG ( "Could not open filesystem on %s\n",
		      efi_handle_name ( handle ) );
		goto err_filesystem;
	}

	/* Open root directory */
	if ( ( efirc = u.fs->OpenVolume ( u.fs, &root ) ) != 0 ) {
		rc = -1;
		DBG ( "Could not open volume on %s\n",
		      efi_handle_name ( handle ) );
		goto err_volume;
	}

	/* Get length of file system information */
	size = 0;
	root->GetInfo ( root, &efi_file_system_info_id, &size, NULL );

	/* Allocate file system information */
	info = malloc ( size );
	if ( ! info ) {
		rc = -1;
		goto err_alloc_info;
	}

	/* Get file system information */
	if ( ( efirc = root->GetInfo ( root, &efi_file_system_info_id, &size,
				       info ) ) != 0 ) {
		rc = -1;
		DBG ( "could not get file system info on %s\n",
		      efi_handle_name ( handle ) );
		goto err_get_info;
	}
	DBG ( "Found %s with label \"%ls\"\n",
	      efi_handle_name ( handle ), info->VolumeLabel );

	snprintf ( label_buf, label_buf_size, "%ls", info->VolumeLabel );

	/* Success */
	rc = 0;

 err_get_info:
	free ( info );
 err_alloc_info:
	root->Close ( root );
 err_volume:
	bs->CloseProtocol ( handle, &efi_simple_file_system_protocol_guid,
			     efi_image_handle, handle );
 err_filesystem:
	return rc;
}

static int efi_boot_create_map ( void ) {
	EFI_BOOT_SERVICES		*bs = efi_systab->BootServices;
	UINTN				 Count;
	UINTN				 i,j;
	INTN				 NextIndex;
	EFI_STATUS			 efirc;
	EFI_HANDLE			*TmpSimpleFSHandleList;
	EFI_DEVICE_PATH_PROTOCOL	**TmpDevicePathList;
	VOID				*buffer;
	const char			*path2;
	char				 path1buf[512]; // 512 is the max buf
							// size used internally
							// by efi_devpath_text()

	DevicePathList = NULL;
	DevicePathListNum = 0;
	SimpleFSHandleList = NULL;

	efi_boot_connect_pcibridges ();

	TmpSimpleFSHandleList = efi_boot_get_handlelist ( &efi_simple_file_system_protocol_guid );
	if ( TmpSimpleFSHandleList == NULL ) {
		/* valid - no filesystems found */
		efi_boot_map_initialized = TRUE;
		return 0;
	}

	/* Count number of handles */
	for ( Count = 0 ; TmpSimpleFSHandleList[Count] != NULL ; Count++ );

	/* Allocate our temporary/local device path list */
	if ( ( efirc = bs->AllocatePool ( EfiBootServicesData,
				          ( Count *
                                            sizeof ( EFI_DEVICE_PATH_PROTOCOL* )
					   ),
					  &buffer ) ) != 0 ) {
		DBG ( "EFIBOOT efi_boot_create_map: AllocatePool failed!\n" );
		bs->FreePool ( TmpSimpleFSHandleList );
		efi_boot_map_initialized = TRUE;
		return -1;
	}
	TmpDevicePathList = (EFI_DEVICE_PATH_PROTOCOL **) buffer;

	/* Populate the devpath list */
	for ( i = 0 ; i < Count; i++ ) {
		TmpDevicePathList[i] = efi_boot_get_devpath ( TmpSimpleFSHandleList[i] );
	}

	/* Allocate our global device path list */
	if ( ( efirc = bs->AllocatePool ( EfiBootServicesData,
				          ( Count *
					    sizeof ( EFI_DEVICE_PATH_PROTOCOL* )
					  ),
					  &buffer ) ) != 0 ) {
		DBG ( "EFIBOOT efi_boot_create_map: AllocatePool failed!\n" );
		bs->FreePool ( TmpSimpleFSHandleList );
		bs->FreePool ( TmpDevicePathList );
		efi_boot_map_initialized = TRUE;

		return -1;
	}
	DevicePathList = (EFI_DEVICE_PATH_PROTOCOL **) buffer;

	/* Allocate our global SimpleFSHandle list */
	if ( ( efirc = bs->AllocatePool ( EfiBootServicesData,
				          ( Count *
					    sizeof ( EFI_HANDLE )
					  ),
					  &buffer ) ) != 0 ) {
		DBG ( "EFIBOOT efi_boot_create_map: AllocatePool failed!\n" );
		bs->FreePool ( DevicePathList );
		DevicePathList = NULL;
		bs->FreePool ( TmpSimpleFSHandleList );
		bs->FreePool ( TmpDevicePathList );
		efi_boot_map_initialized = TRUE;

		return -1;
	}
	SimpleFSHandleList = (EFI_HANDLE *) buffer;

	/*
	 * Populate the global SimpleFSHandle and DevicePath list.
	 * For consistency, order the list.
	 * Since each device path begins with PciRoot()/Pci() nodes,
	 * this will essentially give PCI BDF ordering.
	 * Put NULL devicepaths at end of the list (should not happen).
	 */
	for ( i = 0; i < Count; i++ ) {
		NextIndex = -1;
		path1buf[0]='\0';
		for ( j = 0; j < Count; j++ ) {
			if ( TmpDevicePathList[j] == NULL )
				continue;
			path2 = efi_devpath_text ( TmpDevicePathList[j] );
			if ( !path2 )
				continue;

			DBG ( "EFIBOOT %d: next=%d, comparing %s to %s\n",
			     (int) i, (int) NextIndex, path2, path1buf );
			if ( NextIndex == -1 || strncmp ( path2, path1buf,
							  512 )  < 0 ) {
				NextIndex = j;
				strncpy ( path1buf, path2, 512 );
			}
		}
		if ( NextIndex != -1 ) {
			DevicePathList[i] = TmpDevicePathList[NextIndex];
			SimpleFSHandleList[i] = TmpSimpleFSHandleList[NextIndex];
			TmpDevicePathList[NextIndex] = NULL;
		} else {
			DevicePathList[i] = NULL;
		}
	}

	DevicePathListNum = Count;

	bs->FreePool ( TmpSimpleFSHandleList );
	bs->FreePool ( TmpDevicePathList );

	efi_boot_map_initialized = TRUE;

	return 0;
}

static int efi_boot_local_fs ( EFI_DEVICE_PATH_PROTOCOL *dp,
			       const char *filename ) {
	EFI_BOOT_SERVICES		*bs = efi_systab->BootServices;
	EFI_DEVICE_PATH_PROTOCOL	*boot_path;
	FILEPATH_DEVICE_PATH		*filepath;
	EFI_DEVICE_PATH_PROTOCOL	*end;
	size_t				 prefix_len;
	size_t				 filepath_len;
	size_t				 boot_path_len;
	EFI_HANDLE			 image = NULL;
	EFI_STATUS			 efirc;
	int				 rc;

	if ( dp == NULL )
		return -1;

	DBG ( "EFIBOOT efi_boot_local_fs: device path %s\n",
	      efi_devpath_text ( dp ) );

	/* Construct device path for boot image */
	end = efi_path_end ( dp );
	prefix_len =  ( ( (void *) end ) - ( (void *) dp ) );
	filepath_len = ( SIZE_OF_FILEPATH_DEVICE_PATH +
			 ( filename ? ( ( strlen ( filename ) + 1 ) *
			 sizeof ( filepath->PathName[0] ) ):
			 sizeof ( efi_default_boot_filename ) ) );

	boot_path_len = ( prefix_len + filepath_len + sizeof ( *end ) );
	boot_path = zalloc ( boot_path_len );
	if ( !boot_path ) {
		rc = -1;
		goto err_alloc_path;
	}

	memcpy ( boot_path, dp, prefix_len );
	filepath = ( ( (void *) boot_path ) + prefix_len );
	filepath->Header.Type = MEDIA_DEVICE_PATH;
	filepath->Header.SubType = MEDIA_FILEPATH_DP;
	filepath->Header.Length[0] = ( filepath_len & 0xff );
	filepath->Header.Length[1] = ( filepath_len >> 8 );

	if ( filename ) {
		efi_sprintf ( filepath->PathName, "%s", filename );
	} else {
		memcpy ( filepath->PathName, efi_default_boot_filename,
		         sizeof ( efi_default_boot_filename ) );
	}

	end = ( ( (void *) filepath ) + filepath_len );
	end->Type = END_DEVICE_PATH_TYPE;
	end->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	end->Length[0] = sizeof ( *end );

	/* Release SNP devices */
	efi_snp_release ();

	DBG ( "EFIBOOT attempt to load %s\n", efi_devpath_text ( boot_path ) );

	if ( ( efirc = bs->LoadImage ( FALSE, efi_image_handle, boot_path,
				       NULL, 0, &image ) ) != 0 ) {
		rc = -1;
		DBG ( "EFIBOOT failed to load image\n" );
		goto err_load_image;
	}

	DBG ( "EFIBOOT successfully loaded image\n" );
	DBG ( "EFIBOOT trying to start %s\n",
	      efi_devpath_text ( boot_path ) );

	efirc = bs->StartImage ( image, NULL, NULL );
	if ( EFI_ERROR ( efirc ) )
		rc = -1;
	else
		rc = 0;

	DBG ( "EFIBOOT boot image returned: %d\n", rc );

	bs->UnloadImage ( image );

err_load_image:
	efi_snp_claim ();
	free ( boot_path );
err_alloc_path:

	return rc;
}

void efi_boot_display_map ( void ) {
	char vol_label[256];
	UINTN i;
	int rc;

	if ( !efi_boot_map_initialized )
		efi_boot_create_map ();

	printf ( "Drive#\t[Volume Label] Path\n" );
	printf ( "------\t-------------------\n" );
	for ( i = 0 ; i < DevicePathListNum ; i++ ) {
		if ( DevicePathList[i] != NULL ) {
			rc = efi_vol_label ( SimpleFSHandleList[i],
					     vol_label, 256 );
			if ( rc || *vol_label == '\0' )
				strcpy(vol_label, "NO VOLUME LABEL");
			printf ( "%d     \t[%s] %s\n", (int) i, vol_label,
				efi_devpath_text ( DevicePathList[i] ) );
		}
	}
}

int efi_boot_local ( unsigned int drive, const char *filename ) {

	if ( !efi_boot_map_initialized )
		efi_boot_create_map ();

	if ( DevicePathListNum == 0 || drive > ( DevicePathListNum-1 ) ||
	    DevicePathList[drive] == NULL ) {
		printf ( "ERROR: Invalid drive number %#02x\n", drive );
		return -1;
	}

	efi_boot_local_fs ( DevicePathList[drive], filename );

	return 0;
}
