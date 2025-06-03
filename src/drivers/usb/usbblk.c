/*
 * Copyright (C) 2020 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ipxe/usb.h>
#include <ipxe/scsi.h>
#include <ipxe/xfer.h>
#include <ipxe/uri.h>
#include <ipxe/open.h>
#include <ipxe/efi/efi_path.h>
#include "usbblk.h"

/** @file
 *
 * USB mass storage driver
 *
 */

static void usbblk_stop ( struct usbblk_device *usbblk, int rc );

/** List of USB block devices */
static LIST_HEAD ( usbblk_devices );

/******************************************************************************
 *
 * Endpoint management
 *
 ******************************************************************************
 */

/**
 * Open endpoints
 *
 * @v usbblk		USB block device
 * @ret rc		Return status code
 */
static int usbblk_open ( struct usbblk_device *usbblk ) {
	struct usb_device *usb = usbblk->func->usb;
	unsigned int interface = usbblk->func->interface[0];
	int rc;

	/* Sanity checks */
	assert ( ! usbblk->in.open );
	assert ( ! usbblk->out.open );

	/* Issue reset */
	if ( ( rc = usb_control ( usb, USBBLK_RESET, 0, interface,
				  NULL, 0 ) ) != 0 ) {
		DBGC ( usbblk, "USBBLK %s could not issue reset: %s\n",
		       usbblk->func->name, strerror ( rc ) );
		goto err_reset;
	}

	/* Open bulk OUT endpoint */
	if ( ( rc = usb_endpoint_open ( &usbblk->out ) ) != 0 ) {
		DBGC ( usbblk, "USBBLK %s could not open bulk OUT: %s\n",
		       usbblk->func->name, strerror ( rc ) );
		goto err_open_out;
	}

	/* Clear any bulk OUT halt condition */
	if ( ( rc = usb_endpoint_clear_halt ( &usbblk->out ) ) != 0 ) {
		DBGC ( usbblk, "USBBLK %s could not reset bulk OUT: %s\n",
		       usbblk->func->name, strerror ( rc ) );
		goto err_clear_out;
	}

	/* Open bulk IN endpoint */
	if ( ( rc = usb_endpoint_open ( &usbblk->in ) ) != 0 ) {
		DBGC ( usbblk, "USBBLK %s could not open bulk IN: %s\n",
		       usbblk->func->name, strerror ( rc ) );
		goto err_open_in;
	}

	/* Clear any bulk IN halt condition */
	if ( ( rc = usb_endpoint_clear_halt ( &usbblk->in ) ) != 0 ) {
		DBGC ( usbblk, "USBBLK %s could not reset bulk IN: %s\n",
		       usbblk->func->name, strerror ( rc ) );
		goto err_clear_in;
	}

	return 0;

 err_clear_in:
	usb_endpoint_close ( &usbblk->in );
 err_open_in:
 err_clear_out:
	usb_endpoint_close ( &usbblk->out );
 err_open_out:
 err_reset:
	return rc;
}

/**
 * Close endpoints
 *
 * @v usbblk		USB block device
 */
static void usbblk_close ( struct usbblk_device *usbblk ) {

	/* Close bulk OUT endpoint */
	if ( usbblk->out.open )
		usb_endpoint_close ( &usbblk->out );

	/* Close bulk IN endpoint */
	if ( usbblk->in.open )
		usb_endpoint_close ( &usbblk->in );
}

/******************************************************************************
 *
 * Bulk OUT endpoint
 *
 ******************************************************************************
 */

/**
 * Issue bulk OUT command
 *
 * @v usbblk		USB block device
 * @ret rc		Return status code
 */
static int usbblk_out_command ( struct usbblk_device *usbblk ) {
	struct usbblk_command *cmd = &usbblk->cmd;
	struct usbblk_command_wrapper *wrapper;
	struct io_buffer *iobuf;
	int rc;

	/* Sanity checks */
	assert ( cmd->tag );
	assert ( ! ( cmd->scsi.data_in_len && cmd->scsi.data_out_len ) );

	/* Allocate I/O buffer */
	iobuf = alloc_iob ( sizeof ( *wrapper ) );
	if ( ! iobuf ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Populate command */
	wrapper = iob_put ( iobuf, sizeof ( *wrapper ) );
	memset ( wrapper, 0, sizeof ( *wrapper ) );
	wrapper->signature = cpu_to_le32 ( USBBLK_COMMAND_SIGNATURE );
	wrapper->tag = cmd->tag; /* non-endian */
	if ( cmd->scsi.data_out_len ) {
		wrapper->len = cpu_to_le32 ( cmd->scsi.data_out_len );
	} else {
		wrapper->len = cpu_to_le32 ( cmd->scsi.data_in_len );
		wrapper->flags = USB_DIR_IN;
	}
	wrapper->lun = ntohs ( cmd->scsi.lun.u16[0] );
	wrapper->cblen = sizeof ( wrapper->cb );
	memcpy ( wrapper->cb, &cmd->scsi.cdb, sizeof ( wrapper->cb ) );

	/* Issue command */
	if ( ( rc = usb_stream ( &usbblk->out, iobuf, 0 ) ) != 0 ) {
		DBGC ( usbblk, "USBBLK %s bulk OUT could not issue command: "
		       "%s\n", usbblk->func->name, strerror ( rc ) );
		goto err_stream;
	}

	return 0;

 err_stream:
	free_iob ( iobuf );
 err_alloc:
	return rc;
}

/**
 * Send bulk OUT data block
 *
 * @v usbblk		USB block device
 * @ret rc		Return status code
 */
static int usbblk_out_data ( struct usbblk_device *usbblk ) {
	struct usbblk_command *cmd = &usbblk->cmd;
	struct io_buffer *iobuf;
	size_t len;
	int rc;

	/* Calculate length */
	assert ( cmd->tag );
	assert ( cmd->scsi.data_out != NULL );
	assert ( cmd->offset < cmd->scsi.data_out_len );
	len = ( cmd->scsi.data_out_len - cmd->offset );
	if ( len > USBBLK_MAX_LEN )
		len = USBBLK_MAX_LEN;
	assert ( ( len % usbblk->out.mtu ) == 0 );

	/* Allocate I/O buffer */
	iobuf = alloc_iob ( len );
	if ( ! iobuf ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Populate I/O buffer */
	memcpy ( iob_put ( iobuf, len ),
		 ( cmd->scsi.data_out + cmd->offset ), len );

	/* Send data */
	if ( ( rc = usb_stream ( &usbblk->out, iobuf, 0 ) ) != 0 ) {
		DBGC ( usbblk, "USBBLK %s bulk OUT could not send data: %s\n",
		       usbblk->func->name, strerror ( rc ) );
		goto err_stream;
	}

	/* Consume data */
	cmd->offset += len;

	return 0;

 err_stream:
	free_iob ( iobuf );
 err_alloc:
	return rc;
}

/**
 * Refill bulk OUT endpoint
 *
 * @v usbblk		USB block device
 * @ret rc		Return status code
 */
static int usbblk_out_refill ( struct usbblk_device *usbblk ) {
	struct usbblk_command *cmd = &usbblk->cmd;
	int rc;

	/* Sanity checks */
	assert ( cmd->tag );

	/* Refill endpoint */
	while ( ( cmd->offset < cmd->scsi.data_out_len ) &&
		( usbblk->out.fill < USBBLK_MAX_FILL ) ) {
		if ( ( rc = usbblk_out_data ( usbblk ) ) != 0 )
			return rc;
	}

	return 0;
}

/**
 * Complete bulk OUT transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void usbblk_out_complete ( struct usb_endpoint *ep,
				  struct io_buffer *iobuf, int rc ) {
	struct usbblk_device *usbblk =
		container_of ( ep, struct usbblk_device, out );
	struct usbblk_command *cmd = &usbblk->cmd;

	/* Ignore cancellations after closing endpoint */
	if ( ! ep->open )
		goto drop;

	/* Sanity check */
	assert ( cmd->tag );

	/* Check for failures */
	if ( rc != 0 ) {
		DBGC ( usbblk, "USBBLK %s bulk OUT failed: %s\n",
		       usbblk->func->name, strerror ( rc ) );
		goto err;
	}

	/* Trigger refill process, if applicable */
	if ( cmd->offset < cmd->scsi.data_out_len )
		process_add ( &usbblk->process );

 drop:
	/* Free I/O buffer */
	free_iob ( iobuf );

	return;

 err:
	free_iob ( iobuf );
	usbblk_stop ( usbblk, rc );
}

/** Bulk OUT endpoint operations */
static struct usb_endpoint_driver_operations usbblk_out_operations = {
	.complete = usbblk_out_complete,
};

/******************************************************************************
 *
 * Bulk IN endpoint
 *
 ******************************************************************************
 */

/**
 * Handle bulk IN data block
 *
 * @v usbblk		USB block device
 * @v data		Data block
 * @v len		Length of data
 * @ret rc		Return status code
 */
static int usbblk_in_data ( struct usbblk_device *usbblk, const void *data,
			    size_t len ) {
	struct usbblk_command *cmd = &usbblk->cmd;

	/* Sanity checks */
	assert ( cmd->tag );
	assert ( cmd->scsi.data_in != NULL );
	assert ( cmd->offset <= cmd->scsi.data_in_len );
	assert ( len <= ( cmd->scsi.data_in_len - cmd->offset ) );

	/* Store data */
	memcpy ( ( cmd->scsi.data_in + cmd->offset ), data, len );
	cmd->offset += len;

	return 0;
}

/**
 * Handle bulk IN status
 *
 * @v usbblk		USB block device
 * @v data		Status data
 * @v len		Length of status data
 * @ret rc		Return status code
 */
static int usbblk_in_status ( struct usbblk_device *usbblk, const void *data,
			      size_t len ) {
	struct usbblk_command *cmd = &usbblk->cmd;
	const struct usbblk_status_wrapper *stat;

	/* Sanity checks */
	assert ( cmd->tag );

	/* Validate length */
	if ( len < sizeof ( *stat ) ) {
		DBGC ( usbblk, "USBBLK %s bulk IN malformed status:\n",
		       usbblk->func->name );
		DBGC_HDA ( usbblk, 0, data, len );
		return -EIO;
	}
	stat = data;

	/* Validate signature */
	if ( stat->signature != cpu_to_le32 ( USBBLK_STATUS_SIGNATURE ) ) {
		DBGC ( usbblk, "USBBLK %s bulk IN invalid signature %08x:\n",
		       usbblk->func->name, le32_to_cpu ( stat->signature ) );
		DBGC_HDA ( usbblk, 0, stat, sizeof ( *stat ) );
		return -EIO;
	}

	/* Validate tag */
	if ( stat->tag != cmd->tag ) {
		DBGC ( usbblk, "USBBLK %s bulk IN tag mismatch (got %08x, "
		       "expected %08x):\n",
		       usbblk->func->name, stat->tag, cmd->tag );
		DBGC_HDA ( usbblk, 0, stat, sizeof ( *stat ) );
		return -EIO;
	}

	/* Check status */
	if ( stat->status ) {
		DBGC ( usbblk, "USBBLK %s bulk IN status %02x:\n",
		       usbblk->func->name, stat->status );
		DBGC_HDA ( usbblk, 0, stat, sizeof ( *stat ) );
		return -EIO;
	}

	/* Check for residual data */
	if ( stat->residue ) {
		DBGC ( usbblk, "USBBLK %s bulk IN residue %#x:\n",
		       usbblk->func->name, le32_to_cpu ( stat->residue ) );
		return -EIO;
	}

	/* Mark command as complete */
	usbblk_stop ( usbblk, 0 );

	return 0;
}

/**
 * Refill bulk IN endpoint
 *
 * @v usbblk		USB block device
 * @ret rc		Return status code
 */
static int usbblk_in_refill ( struct usbblk_device *usbblk ) {
	struct usbblk_command *cmd = &usbblk->cmd;
	struct usbblk_status_wrapper *stat;
	size_t remaining;
	unsigned int max;
	int rc;

	/* Sanity checks */
	assert ( cmd->tag );

	/* Calculate maximum required refill */
	remaining = sizeof ( *stat );
	if ( cmd->scsi.data_in_len ) {
		assert ( cmd->offset <= cmd->scsi.data_in_len );
		remaining += ( cmd->scsi.data_in_len - cmd->offset );
	}
	max = ( ( remaining + USBBLK_MAX_LEN - 1 ) / USBBLK_MAX_LEN );

	/* Refill bulk IN endpoint */
	if ( ( rc = usb_refill_limit ( &usbblk->in, max ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Complete bulk IN transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void usbblk_in_complete ( struct usb_endpoint *ep,
				 struct io_buffer *iobuf, int rc ) {
	struct usbblk_device *usbblk =
		container_of ( ep, struct usbblk_device, in );
	struct usbblk_command *cmd = &usbblk->cmd;
	size_t remaining;
	size_t len;

	/* Ignore cancellations after closing endpoint */
	if ( ! ep->open )
		goto drop;

	/* Sanity check */
	assert ( cmd->tag );

	/* Handle errors */
	if ( rc != 0 ) {
		DBGC ( usbblk, "USBBLK %s bulk IN failed: %s\n",
		       usbblk->func->name, strerror ( rc ) );
		goto err;
	}

	/* Trigger refill process */
	process_add ( &usbblk->process );

	/* Handle data portion, if any */
	if ( cmd->scsi.data_in_len ) {
		assert ( cmd->offset <= cmd->scsi.data_in_len );
		remaining = ( cmd->scsi.data_in_len - cmd->offset );
		len = iob_len ( iobuf );
		if ( len > remaining )
			len = remaining;
		if ( len ) {
			if ( ( rc = usbblk_in_data ( usbblk, iobuf->data,
						     len ) ) != 0 )
				goto err;
			iob_pull ( iobuf, len );
		}
	}

	/* Handle status portion, if any */
	len = iob_len ( iobuf );
	if ( len ) {
		if ( ( rc = usbblk_in_status ( usbblk, iobuf->data,
					       len ) ) != 0 )
			goto err;
	}

 drop:
	/* Free I/O buffer */
	free_iob ( iobuf );

	return;

 err:
	free_iob ( iobuf );
	usbblk_stop ( usbblk, rc );
}

/** Bulk IN endpoint operations */
static struct usb_endpoint_driver_operations usbblk_in_operations = {
	.complete = usbblk_in_complete,
};

/******************************************************************************
 *
 * Refill process
 *
 ******************************************************************************
 */

/**
 * Refill endpoints
 *
 * @v usbblk		USB block device
 */
static void usbblk_step ( struct usbblk_device *usbblk ) {

	/* Refill bulk OUT endpoint */
	usbblk_out_refill ( usbblk );

	/* Refill bulk IN endpoint */
	usbblk_in_refill ( usbblk );
}

/** Refill process descriptor */
static struct process_descriptor usbblk_process_desc =
	PROC_DESC ( struct usbblk_device, process, usbblk_step );

/******************************************************************************
 *
 * SCSI command management
 *
 ******************************************************************************
 */

/** Next command tag */
static uint16_t usbblk_tag;

/**
 * Stop SCSI command
 *
 * @v usbblk		USB block device
 * @v rc		Reason for stop
 */
static void usbblk_stop ( struct usbblk_device *usbblk, int rc ) {

	/* Stop process */
	process_del ( &usbblk->process );

	/* Reset command */
	memset ( &usbblk->cmd, 0, sizeof ( usbblk->cmd ) );

	/* Close endpoints if an error occurred */
	if ( rc != 0 ) {
		DBGC ( usbblk, "USBBLK %s closing for error recovery\n",
		       usbblk->func->name );
		usbblk_close ( usbblk );
	}

	/* Terminate command */
	intf_restart ( &usbblk->data, rc );
}

/**
 * Start new SCSI command
 *
 * @v usbblk		USB block device
 * @v scsicmd		SCSI command
 * @ret rc		Return status code
 */
static int usbblk_start ( struct usbblk_device *usbblk,
			  struct scsi_cmd *scsicmd ) {
	struct usbblk_command *cmd = &usbblk->cmd;
	int rc;

	/* Fail if command is in progress */
	if ( cmd->tag ) {
		rc = -EBUSY;
		DBGC ( usbblk, "USBBLK %s cannot support multiple commands\n",
		       usbblk->func->name );
		goto err_busy;
	}

	/* Refuse bidirectional commands */
	if ( scsicmd->data_in_len && scsicmd->data_out_len ) {
		rc = -EOPNOTSUPP;
		DBGC ( usbblk, "USBBLK %s cannot support bidirectional "
		       "commands\n", usbblk->func->name );
		goto err_bidirectional;
	}

	/* Sanity checks */
	assert ( ! process_running ( &usbblk->process ) );
	assert ( cmd->offset == 0 );

	/* Initialise command */
	memcpy ( &cmd->scsi, scsicmd, sizeof ( cmd->scsi ) );
	cmd->tag = ( USBBLK_TAG_MAGIC | ++usbblk_tag );

	/* Issue bulk OUT command */
	if ( ( rc = usbblk_out_command ( usbblk ) ) != 0 )
		goto err_command;

	/* Start refill process */
	process_add ( &usbblk->process );

	return 0;

 err_command:
	memset ( &usbblk->cmd, 0, sizeof ( usbblk->cmd ) );
 err_bidirectional:
 err_busy:
	return rc;
}

/******************************************************************************
 *
 * SCSI interfaces
 *
 ******************************************************************************
 */

/** SCSI data interface operations */
static struct interface_operation usbblk_data_operations[] = {
	INTF_OP ( intf_close, struct usbblk_device *, usbblk_stop ),
};

/** SCSI data interface descriptor */
static struct interface_descriptor usbblk_data_desc =
	INTF_DESC ( struct usbblk_device, data, usbblk_data_operations );

/**
 * Check SCSI command flow-control window
 *
 * @v usbblk		USB block device
 * @ret len		Length of window
 */
static size_t usbblk_scsi_window ( struct usbblk_device *usbblk ) {
	struct usbblk_command *cmd = &usbblk->cmd;

	/* Allow a single command if no command is currently in progress */
	return ( cmd->tag ? 0 : 1 );
}

/**
 * Issue SCSI command
 *
 * @v usbblk		USB block device
 * @v data		SCSI data interface
 * @v scsicmd		SCSI command
 * @ret tag		Command tag, or negative error
 */
static int usbblk_scsi_command ( struct usbblk_device *usbblk,
				 struct interface *data,
				 struct scsi_cmd *scsicmd ) {
	struct usbblk_command *cmd = &usbblk->cmd;
	int rc;

	/* (Re)open endpoints if needed */
	if ( ( ! usbblk->in.open ) && ( ( rc = usbblk_open ( usbblk ) ) != 0 ) )
		goto err_open;

	/* Start new command */
	if ( ( rc = usbblk_start ( usbblk, scsicmd ) ) != 0 )
		goto err_start;

	/* Attach to parent interface and return */
	intf_plug_plug ( &usbblk->data, data );
	return cmd->tag;

	usbblk_stop ( usbblk, rc );
 err_start:
	usbblk_close ( usbblk );
 err_open:
	return rc;
}

/**
 * Close SCSI interface
 *
 * @v usbblk		USB block device
 * @v rc		Reason for close
 */
static void usbblk_scsi_close ( struct usbblk_device *usbblk, int rc ) {

	/* Restart interfaces */
	intfs_restart ( rc, &usbblk->scsi, &usbblk->data, NULL );

	/* Stop any in-progress command */
	usbblk_stop ( usbblk, rc );

	/* Close endpoints */
	usbblk_close ( usbblk );

	/* Flag as closed */
	usbblk->opened = 0;
}

/**
 * Describe as an EFI device path
 *
 * @v usbblk		USB block device
 * @ret path		EFI device path, or NULL on error
 */
static EFI_DEVICE_PATH_PROTOCOL *
usbblk_efi_describe ( struct usbblk_device *usbblk ) {

	return efi_usb_path ( usbblk->func );
}

/** SCSI command interface operations */
static struct interface_operation usbblk_scsi_operations[] = {
	INTF_OP ( scsi_command, struct usbblk_device *, usbblk_scsi_command ),
	INTF_OP ( xfer_window, struct usbblk_device *, usbblk_scsi_window ),
	INTF_OP ( intf_close, struct usbblk_device *, usbblk_scsi_close ),
	EFI_INTF_OP ( efi_describe, struct usbblk_device *,
		      usbblk_efi_describe ),
};

/** SCSI command interface descriptor */
static struct interface_descriptor usbblk_scsi_desc =
	INTF_DESC ( struct usbblk_device, scsi, usbblk_scsi_operations );

/******************************************************************************
 *
 * SAN device interface
 *
 ******************************************************************************
 */

/**
 * Find USB block device
 *
 * @v name		USB block device name
 * @ret usbblk		USB block device, or NULL
 */
static struct usbblk_device * usbblk_find ( const char *name ) {
	struct usbblk_device *usbblk;

	/* Look for matching device */
	list_for_each_entry ( usbblk, &usbblk_devices, list ) {
		if ( strcmp ( usbblk->func->name, name ) == 0 )
			return usbblk;
	}

	return NULL;
}

/**
 * Open USB block device URI
 *
 * @v parent		Parent interface
 * @v uri		URI
 * @ret rc		Return status code
 */
static int usbblk_open_uri ( struct interface *parent, struct uri *uri ) {
	static struct scsi_lun lun;
	struct usbblk_device *usbblk;
	int rc;

	/* Sanity check */
	if ( ! uri->opaque )
		return -EINVAL;

	/* Find matching device */
	usbblk = usbblk_find ( uri->opaque );
	if ( ! usbblk )
		return -ENOENT;

	/* Fail if device is already open */
	if ( usbblk->opened )
		return -EBUSY;

	/* Open SCSI device */
	if ( ( rc = scsi_open ( parent, &usbblk->scsi, &lun ) ) != 0 ) {
		DBGC ( usbblk, "USBBLK %s could not open SCSI device: %s\n",
		       usbblk->func->name, strerror ( rc ) );
		return rc;
	}

	/* Mark as opened */
	usbblk->opened = 1;

	return 0;
}

/** USB block device URI opener */
struct uri_opener usbblk_uri_opener __uri_opener = {
	.scheme = "usb",
	.open = usbblk_open_uri,
};

/******************************************************************************
 *
 * USB interface
 *
 ******************************************************************************
 */

/**
 * Probe device
 *
 * @v func		USB function
 * @v config		Configuration descriptor
 * @ret rc		Return status code
 */
static int usbblk_probe ( struct usb_function *func,
			  struct usb_configuration_descriptor *config ) {
	struct usb_device *usb = func->usb;
	struct usbblk_device *usbblk;
	struct usb_interface_descriptor *desc;
	int rc;

	/* Allocate and initialise structure */
	usbblk = zalloc ( sizeof ( *usbblk ) );
	if ( ! usbblk ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	usbblk->func = func;
	usb_endpoint_init ( &usbblk->out, usb, &usbblk_out_operations );
	usb_endpoint_init ( &usbblk->in, usb, &usbblk_in_operations );
	usb_refill_init ( &usbblk->in, 0, USBBLK_MAX_LEN, USBBLK_MAX_FILL );
	intf_init ( &usbblk->scsi, &usbblk_scsi_desc, &usbblk->refcnt );
	intf_init ( &usbblk->data, &usbblk_data_desc, &usbblk->refcnt );
	process_init_stopped ( &usbblk->process, &usbblk_process_desc,
			       &usbblk->refcnt );

	/* Locate interface descriptor */
	desc = usb_interface_descriptor ( config, func->interface[0], 0 );
	if ( ! desc ) {
		DBGC ( usbblk, "USBBLK %s missing interface descriptor\n",
		       usbblk->func->name );
		rc = -ENOENT;
		goto err_desc;
	}

	/* Describe endpoints */
	if ( ( rc = usb_endpoint_described ( &usbblk->out, config, desc,
					     USB_BULK_OUT, 0 ) ) != 0 ) {
		DBGC ( usbblk, "USBBLK %s could not describe bulk OUT: %s\n",
		       usbblk->func->name, strerror ( rc ) );
		goto err_out;
	}
	if ( ( rc = usb_endpoint_described ( &usbblk->in, config, desc,
					     USB_BULK_IN, 0 ) ) != 0 ) {
		DBGC ( usbblk, "USBBLK %s could not describe bulk IN: %s\n",
		       usbblk->func->name, strerror ( rc ) );
		goto err_in;
	}

	/* Add to list of devices */
	list_add_tail ( &usbblk->list, &usbblk_devices );

	usb_func_set_drvdata ( func, usbblk );
	return 0;

 err_in:
 err_out:
 err_desc:
	ref_put ( &usbblk->refcnt );
 err_alloc:
	return rc;
}

/**
 * Remove device
 *
 * @v func		USB function
 */
static void usbblk_remove ( struct usb_function *func ) {
	struct usbblk_device *usbblk = usb_func_get_drvdata ( func );

	/* Remove from list of devices */
	list_del ( &usbblk->list );

	/* Close all interfaces */
	usbblk_scsi_close ( usbblk, -ENODEV );

	/* Shut down interfaces */
	intfs_shutdown ( -ENODEV, &usbblk->scsi, &usbblk->data, NULL );

	/* Drop reference */
	ref_put ( &usbblk->refcnt );
}

/** Mass storage class device IDs */
static struct usb_device_id usbblk_ids[] = {
	{
		.name = "usbblk",
		.vendor = USB_ANY_ID,
		.product = USB_ANY_ID,
	},
};

/** Mass storage driver */
struct usb_driver usbblk_driver __usb_driver = {
	.ids = usbblk_ids,
	.id_count = ( sizeof ( usbblk_ids ) / sizeof ( usbblk_ids[0] ) ),
	.class = USB_CLASS_ID ( USB_CLASS_MSC, USB_SUBCLASS_MSC_SCSI,
				USB_PROTOCOL_MSC_BULK ),
	.score = USB_SCORE_NORMAL,
	.probe = usbblk_probe,
	.remove = usbblk_remove,
};
