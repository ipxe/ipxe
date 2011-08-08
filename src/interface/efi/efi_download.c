/*
 * Copyright (C) 2010 VMware, Inc.  All Rights Reserved.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdlib.h>
#include <string.h>
#include <ipxe/open.h>
#include <ipxe/process.h>
#include <ipxe/iobuf.h>
#include <ipxe/xfer.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/ipxe_download.h>

/** iPXE download protocol GUID */
static EFI_GUID ipxe_download_protocol_guid
	= IPXE_DOWNLOAD_PROTOCOL_GUID;

/** A single in-progress file */
struct efi_download_file {
	/** Data transfer interface that provides downloaded data */
	struct interface xfer;

	/** Current file position */
	size_t pos;

	/** Data callback */
	IPXE_DOWNLOAD_DATA_CALLBACK data_callback;

	/** Finish callback */
	IPXE_DOWNLOAD_FINISH_CALLBACK finish_callback;

	/** Callback context */
	void *context;
};

/* xfer interface */

/**
 * Transfer finished or was aborted
 *
 * @v file		Data transfer file
 * @v rc		Reason for close
 */
static void efi_download_close ( struct efi_download_file *file, int rc ) {

	file->finish_callback ( file->context, RC_TO_EFIRC ( rc ) );

	intf_shutdown ( &file->xfer, rc );
}

/**
 * Process received data
 *
 * @v file		Data transfer file
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int efi_download_deliver_iob ( struct efi_download_file *file,
				      struct io_buffer *iobuf,
				      struct xfer_metadata *meta ) {
	EFI_STATUS efirc;
	size_t len = iob_len ( iobuf );

	/* Calculate new buffer position */
	if ( meta->flags & XFER_FL_ABS_OFFSET )
		file->pos = 0;
	file->pos += meta->offset;

	/* Call out to the data handler */
	efirc = file->data_callback ( file->context, iobuf->data,
				      len, file->pos );

	/* Update current buffer position */
	file->pos += len;

	free_iob ( iobuf );
	return EFIRC_TO_RC ( efirc );
}

/** Data transfer interface operations */
static struct interface_operation efi_xfer_operations[] = {
	INTF_OP ( xfer_deliver, struct efi_download_file *, efi_download_deliver_iob ),
	INTF_OP ( intf_close, struct efi_download_file *, efi_download_close ),
};

/** EFI download data transfer interface descriptor */
static struct interface_descriptor efi_download_file_xfer_desc =
	INTF_DESC ( struct efi_download_file, xfer, efi_xfer_operations );

/**
 * Start downloading a file, and register callback functions to handle the
 * download.
 *
 * @v This		iPXE Download Protocol instance
 * @v Url		URL to download from
 * @v DataCallback	Callback that will be invoked when data arrives
 * @v FinishCallback	Callback that will be invoked when the download ends
 * @v Context		Context passed to the Data and Finish callbacks
 * @v File		Token that can be used to abort the download
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
efi_download_start ( IPXE_DOWNLOAD_PROTOCOL *This __unused,
		     CHAR8 *Url,
		     IPXE_DOWNLOAD_DATA_CALLBACK DataCallback,
		     IPXE_DOWNLOAD_FINISH_CALLBACK FinishCallback,
		     VOID *Context,
		     IPXE_DOWNLOAD_FILE *File ) {
	struct efi_download_file *file;
	int rc;

	file = malloc ( sizeof ( struct efi_download_file ) );
	if ( file == NULL ) {
		return EFI_OUT_OF_RESOURCES;
	}

	intf_init ( &file->xfer, &efi_download_file_xfer_desc, NULL );
	rc = xfer_open ( &file->xfer, LOCATION_URI_STRING, Url );
	if ( rc ) {
		free ( file );
		return RC_TO_EFIRC ( rc );
	}

	file->pos = 0;
	file->data_callback = DataCallback;
	file->finish_callback = FinishCallback;
	file->context = Context;
	*File = file;
	return EFI_SUCCESS;
}

/**
 * Forcibly abort downloading a file that is currently in progress.
 *
 * It is not safe to call this function after the Finish callback has executed.
 *
 * @v This		iPXE Download Protocol instance
 * @v File		Token obtained from Start
 * @v Status		Reason for aborting the download
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
efi_download_abort ( IPXE_DOWNLOAD_PROTOCOL *This __unused,
		     IPXE_DOWNLOAD_FILE File,
		     EFI_STATUS Status ) {
	struct efi_download_file *file = File;

	efi_download_close ( file, EFIRC_TO_RC ( Status ) );
	return EFI_SUCCESS;
}

/**
 * Poll for more data from iPXE. This function will invoke the registered
 * callbacks if data is available or if downloads complete.
 *
 * @v This		iPXE Download Protocol instance
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
efi_download_poll ( IPXE_DOWNLOAD_PROTOCOL *This __unused ) {
	step();
	return EFI_SUCCESS;
}

/** Publicly exposed iPXE download protocol */
static IPXE_DOWNLOAD_PROTOCOL ipxe_download_protocol_interface = {
	.Start = efi_download_start,
	.Abort = efi_download_abort,
	.Poll = efi_download_poll
};

/**
 * Create a new device handle with a iPXE download protocol attached to it.
 *
 * @v device_handle	Newly created device handle (output)
 * @ret rc		Return status code
 */
int efi_download_install ( EFI_HANDLE *device_handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;
	EFI_HANDLE handle = NULL;
	if (efi_loaded_image->DeviceHandle) { /* TODO: ensure handle is the NIC (maybe efi_image has a better way to indicate the handle doing SNP?) */
		handle = efi_loaded_image->DeviceHandle;
	}

	DBG ( "Installing ipxe protocol interface (%p)... ",
	      &ipxe_download_protocol_interface );
	efirc = bs->InstallMultipleProtocolInterfaces (
			&handle,
			&ipxe_download_protocol_guid,
			&ipxe_download_protocol_interface,
			NULL );
	if ( efirc ) {
		DBG ( "failed (%s)\n", efi_strerror ( efirc ) );
		return EFIRC_TO_RC ( efirc );
	}

	DBG ( "success (%p)\n", handle );
	*device_handle = handle;
	return 0;
}

/**
 * Remove the iPXE download protocol from the given handle, and if nothing
 * else is attached, destroy the handle.
 *
 * @v device_handle	EFI device handle to remove from
 */
void efi_download_uninstall ( EFI_HANDLE device_handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	bs->UninstallMultipleProtocolInterfaces (
			device_handle,
			ipxe_download_protocol_guid,
			ipxe_download_protocol_interface );
}
