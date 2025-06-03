/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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

/** @file
 *
 * EFI protocol opening and closing
 *
 * The UEFI model for opening and closing protocols is broken by
 * design and cannot be repaired.
 *
 * Calling OpenProtocol() to obtain a protocol interface pointer does
 * not, in general, provide any guarantees about the lifetime of that
 * pointer.  It is theoretically possible that the pointer has already
 * become invalid by the time that OpenProtocol() returns the pointer
 * to its caller.  (This can happen when a USB device is physically
 * removed, for example.)
 *
 * Various UEFI design flaws make it occasionally necessary to hold on
 * to a protocol interface pointer despite the total lack of
 * guarantees that the pointer will remain valid.
 *
 * The UEFI driver model overloads the semantics of OpenProtocol() to
 * accommodate the use cases of recording a driver attachment (which
 * is modelled as opening a protocol with EFI_OPEN_PROTOCOL_BY_DRIVER
 * attributes) and recording the existence of a related child
 * controller (which is modelled as opening a protocol with
 * EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER attributes).
 *
 * The parameters defined for CloseProtocol() are not sufficient to
 * allow the implementation to precisely identify the matching call to
 * OpenProtocol().  While the UEFI model appears to allow for matched
 * open and close pairs, this is merely an illusion.  Calling
 * CloseProtocol() will delete *all* matching records in the protocol
 * open information tables.
 *
 * Since the parameters defined for CloseProtocol() do not include the
 * attributes passed to OpenProtocol(), this means that a matched
 * open/close pair using EFI_OPEN_PROTOCOL_GET_PROTOCOL can
 * inadvertently end up deleting the record that defines a driver
 * attachment or the existence of a child controller.  This in turn
 * can cause some very unexpected side effects, such as allowing other
 * UEFI drivers to start controlling hardware to which iPXE believes
 * it has exclusive access.  This rarely ends well.
 *
 * To prevent this kind of inadvertent deletion, we establish a
 * convention for four different types of protocol opening:
 *
 * - ephemeral opens: always opened with ControllerHandle = NULL
 *
 * - unsafe opens: always opened with ControllerHandle = AgentHandle
 *
 * - by-driver opens: always opened with ControllerHandle = Handle
 *
 * - by-child opens: always opened with ControllerHandle != Handle
 *
 * This convention ensures that the four types of open never overlap
 * within the set of parameters defined for CloseProtocol(), and so a
 * close of one type cannot inadvertently delete the record
 * corresponding to a different type.
 */

#include <assert.h>
#include <errno.h>
#include <ipxe/efi/efi.h>

/**
 * Open (or test) protocol for ephemeral use
 *
 * @v handle		EFI handle
 * @v protocol		Protocol GUID
 * @v interface		Protocol interface pointer to fill in (or NULL to test)
 * @ret rc		Return status code
 */
int efi_open_untyped ( EFI_HANDLE handle, EFI_GUID *protocol,
		       void **interface ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE agent = efi_image_handle;
	EFI_HANDLE controller;
	unsigned int attributes;
	EFI_STATUS efirc;
	int rc;

	/* Sanity checks */
	assert ( handle != NULL );
	assert ( protocol != NULL );

	/* Open protocol
	 *
	 * We set ControllerHandle to NULL to avoid collisions with
	 * other open types.
	 */
	controller = NULL;
	attributes = ( interface ? EFI_OPEN_PROTOCOL_GET_PROTOCOL :
		       EFI_OPEN_PROTOCOL_TEST_PROTOCOL );
	if ( ( efirc = bs->OpenProtocol ( handle, protocol, interface, agent,
					  controller, attributes ) ) != 0 ) {
		rc = -EEFI ( efirc );
		if ( interface )
			*interface = NULL;
		return rc;
	}

	/* Close protocol immediately
	 *
	 * While it may seem prima facie unsafe to use a protocol
	 * after closing it, UEFI doesn't actually give us any safety
	 * even while the protocol is nominally open.  Opening a
	 * protocol with EFI_OPEN_PROTOCOL_GET_PROTOCOL attributes
	 * does not in any way ensure that the interface pointer
	 * remains valid: there are no locks or notifications
	 * associated with these "opens".
	 *
	 * The only way to obtain a (partially) guaranteed persistent
	 * interface pointer is to open the protocol with the
	 * EFI_OPEN_PROTOCOL_BY_DRIVER attributes.  This is not
	 * possible in the general case, since UEFI permits only a
	 * single image at a time to have the protocol opened with
	 * these attributes.
	 *
	 * We can therefore obtain at best an ephemeral interface
	 * pointer: one that is guaranteed to remain valid only for as
	 * long as we do not relinquish the thread of control.
	 *
	 * (Since UEFI permits calls to UninstallProtocolInterface()
	 * at levels up to and including TPL_NOTIFY, this means that
	 * we technically cannot rely on the pointer remaining valid
	 * unless the caller is itself running at TPL_NOTIFY.  This is
	 * clearly impractical, and large portions of the EDK2
	 * codebase presume that using EFI_OPEN_PROTOCOL_GET_PROTOCOL
	 * is safe at lower TPLs.)
	 *
	 * Closing is not strictly necessary for protocols opened
	 * ephemerally (i.e. using EFI_OPEN_PROTOCOL_GET_PROTOCOL or
	 * EFI_OPEN_PROTOCOL_TEST_PROTOCOL), but avoids polluting the
	 * protocol open information tables with stale data.
	 *
	 * Closing immediately also simplifies the callers' code
	 * paths, since they do not need to worry about closing the
	 * protocol.
	 *
	 * The overall effect is equivalent to using HandleProtocol(),
	 * but without the associated pollution of the protocol open
	 * information tables, and with improved traceability.
	 */
	bs->CloseProtocol ( handle, protocol, agent, controller );

	return 0;
}

/**
 * Open protocol for unsafe persistent use
 *
 * @v handle		EFI handle
 * @v protocol		Protocol GUID
 * @v interface		Protocol interface pointer to fill in
 * @ret rc		Return status code
 */
int efi_open_unsafe_untyped ( EFI_HANDLE handle, EFI_GUID *protocol,
			      void **interface ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE agent = efi_image_handle;
	EFI_HANDLE controller;
	unsigned int attributes;
	EFI_STATUS efirc;
	int rc;

	/* Sanity checks */
	assert ( handle != NULL );
	assert ( protocol != NULL );
	assert ( interface != NULL );

	/* Open protocol
	 *
	 * We set ControllerHandle equal to AgentHandle to avoid
	 * collisions with other open types.
	 */
	controller = agent;
	attributes = EFI_OPEN_PROTOCOL_GET_PROTOCOL;
	if ( ( efirc = bs->OpenProtocol ( handle, protocol, interface, agent,
					  controller, attributes ) ) != 0 ) {
		rc = -EEFI ( efirc );
		*interface = NULL;
		return rc;
	}

	return 0;
}

/**
 * Close protocol opened for unsafe persistent use
 *
 * @v handle		EFI handle
 * @v protocol		Protocol GUID
 * @v child		Child controller handle
 */
void efi_close_unsafe ( EFI_HANDLE handle, EFI_GUID *protocol ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE agent = efi_image_handle;
	EFI_HANDLE controller;

	/* Sanity checks */
	assert ( handle != NULL );
	assert ( protocol != NULL );

	/* Close protocol */
	controller = agent;
	bs->CloseProtocol ( handle, protocol, agent, controller );
}

/**
 * Open protocol for persistent use by a driver
 *
 * @v handle		EFI handle
 * @v protocol		Protocol GUID
 * @v interface		Protocol interface pointer to fill in
 * @ret rc		Return status code
 */
int efi_open_by_driver_untyped ( EFI_HANDLE handle, EFI_GUID *protocol,
				 void **interface ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE agent = efi_image_handle;
	EFI_HANDLE controller;
	unsigned int attributes;
	EFI_STATUS efirc;
	int rc;

	/* Sanity checks */
	assert ( handle != NULL );
	assert ( protocol != NULL );
	assert ( interface != NULL );

	/* Open protocol
	 *
	 * We set ControllerHandle equal to Handle to avoid collisions
	 * with other open types.
	 */
	controller = handle;
	attributes = ( EFI_OPEN_PROTOCOL_BY_DRIVER |
		       EFI_OPEN_PROTOCOL_EXCLUSIVE );
	if ( ( efirc = bs->OpenProtocol ( handle, protocol, interface, agent,
					  controller, attributes ) ) != 0 ) {
		rc = -EEFI ( efirc );
		*interface = NULL;
		return rc;
	}

	return 0;
}

/**
 * Close protocol opened for persistent use by a driver
 *
 * @v handle		EFI handle
 * @v protocol		Protocol GUID
 */
void efi_close_by_driver ( EFI_HANDLE handle, EFI_GUID *protocol ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE agent = efi_image_handle;
	EFI_HANDLE controller;

	/* Sanity checks */
	assert ( handle != NULL );
	assert ( protocol != NULL );

	/* Close protocol */
	controller = handle;
	bs->CloseProtocol ( handle, protocol, agent, controller );
}

/**
 * Open protocol for persistent use by a child controller
 *
 * @v handle		EFI handle
 * @v protocol		Protocol GUID
 * @v child		Child controller handle
 * @v interface		Protocol interface pointer to fill in
 * @ret rc		Return status code
 */
int efi_open_by_child_untyped ( EFI_HANDLE handle, EFI_GUID *protocol,
				EFI_HANDLE child, void **interface ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE agent = efi_image_handle;
	EFI_HANDLE controller;
	unsigned int attributes;
	EFI_STATUS efirc;
	int rc;

	/* Sanity checks */
	assert ( handle != NULL );
	assert ( protocol != NULL );
	assert ( child != NULL );
	assert ( interface != NULL );

	/* Open protocol
	 *
	 * We set ControllerHandle to a non-NULL value distinct from
	 * both Handle and AgentHandle to avoid collisions with other
	 * open types.
	 */
	controller = child;
	assert ( controller != handle );
	assert ( controller != agent );
	attributes = EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER;
	if ( ( efirc = bs->OpenProtocol ( handle, protocol, interface, agent,
					  controller, attributes ) ) != 0 ) {
		rc = -EEFI ( efirc );
		*interface = NULL;
		return rc;
	}

	return 0;
}

/**
 * Close protocol opened for persistent use by a child controller
 *
 * @v handle		EFI handle
 * @v protocol		Protocol GUID
 * @v child		Child controller handle
 */
void efi_close_by_child ( EFI_HANDLE handle, EFI_GUID *protocol,
			  EFI_HANDLE child ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE agent = efi_image_handle;
	EFI_HANDLE controller;

	/* Sanity checks */
	assert ( handle != NULL );
	assert ( protocol != NULL );
	assert ( child != NULL );

	/* Close protocol */
	controller = child;
	assert ( controller != handle );
	assert ( controller != agent );
	bs->CloseProtocol ( handle, protocol, agent, controller );
}
