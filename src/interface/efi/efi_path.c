/*
 * Copyright (C) 2020 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <byteswap.h>
#include <ipxe/netdevice.h>
#include <ipxe/vlan.h>
#include <ipxe/tcpip.h>
#include <ipxe/uri.h>
#include <ipxe/iscsi.h>
#include <ipxe/aoe.h>
#include <ipxe/fcp.h>
#include <ipxe/ib_srp.h>
#include <ipxe/usb.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_path.h>

/** @file
 *
 * EFI device paths
 *
 */

/**
 * Find end of device path
 *
 * @v path		Path to device
 * @ret path_end	End of device path
 */
EFI_DEVICE_PATH_PROTOCOL * efi_path_end ( EFI_DEVICE_PATH_PROTOCOL *path ) {

	while ( path->Type != END_DEVICE_PATH_TYPE ) {
		path = ( ( ( void * ) path ) +
			 /* There's this amazing new-fangled thing known as
			  * a UINT16, but who wants to use one of those? */
			 ( ( path->Length[1] << 8 ) | path->Length[0] ) );
	}

	return path;
}

/**
 * Find length of device path (excluding terminator)
 *
 * @v path		Path to device
 * @ret path_len	Length of device path
 */
size_t efi_path_len ( EFI_DEVICE_PATH_PROTOCOL *path ) {
	EFI_DEVICE_PATH_PROTOCOL *end = efi_path_end ( path );

	return ( ( ( void * ) end ) - ( ( void * ) path ) );
}

/**
 * Concatenate EFI device paths
 *
 * @v ...		List of device paths (NULL terminated)
 * @ret path		Concatenated device path, or NULL on error
 *
 * The caller is responsible for eventually calling free() on the
 * allocated device path.
 */
EFI_DEVICE_PATH_PROTOCOL * efi_paths ( EFI_DEVICE_PATH_PROTOCOL *first, ... ) {
	EFI_DEVICE_PATH_PROTOCOL *path;
	EFI_DEVICE_PATH_PROTOCOL *src;
	EFI_DEVICE_PATH_PROTOCOL *dst;
	EFI_DEVICE_PATH_PROTOCOL *end;
	va_list args;
	size_t len;

	/* Calculate device path length */
	va_start ( args, first );
	len = 0;
	src = first;
	while ( src ) {
		len += efi_path_len ( src );
		src = va_arg ( args, EFI_DEVICE_PATH_PROTOCOL * );
	}
	va_end ( args );

	/* Allocate device path */
	path = zalloc ( len + sizeof ( *end ) );
	if ( ! path )
		return NULL;

	/* Populate device path */
	va_start ( args, first );
	dst = path;
	src = first;
	while ( src ) {
		len = efi_path_len ( src );
		memcpy ( dst, src, len );
		dst = ( ( ( void * ) dst ) + len );
		src = va_arg ( args, EFI_DEVICE_PATH_PROTOCOL * );
	}
	va_end ( args );
	end = dst;
	end->Type = END_DEVICE_PATH_TYPE;
	end->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	end->Length[0] = sizeof ( *end );

	return path;
}

/**
 * Construct EFI device path for network device
 *
 * @v netdev		Network device
 * @ret path		EFI device path, or NULL on error
 *
 * The caller is responsible for eventually calling free() on the
 * allocated device path.
 */
EFI_DEVICE_PATH_PROTOCOL * efi_netdev_path ( struct net_device *netdev ) {
	struct efi_device *efidev;
	EFI_DEVICE_PATH_PROTOCOL *path;
	MAC_ADDR_DEVICE_PATH *macpath;
	VLAN_DEVICE_PATH *vlanpath;
	EFI_DEVICE_PATH_PROTOCOL *end;
	unsigned int tag;
	size_t prefix_len;
	size_t len;

	/* Find parent EFI device */
	efidev = efidev_parent ( netdev->dev );
	if ( ! efidev )
		return NULL;

	/* Calculate device path length */
	prefix_len = efi_path_len ( efidev->path );
	len = ( prefix_len + sizeof ( *macpath ) + sizeof ( *vlanpath ) +
		sizeof ( *end ) );

	/* Allocate device path */
	path = zalloc ( len );
	if ( ! path )
		return NULL;

	/* Construct device path */
	memcpy ( path, efidev->path, prefix_len );
	macpath = ( ( ( void * ) path ) + prefix_len );
	macpath->Header.Type = MESSAGING_DEVICE_PATH;
	macpath->Header.SubType = MSG_MAC_ADDR_DP;
	macpath->Header.Length[0] = sizeof ( *macpath );
	assert ( netdev->ll_protocol->ll_addr_len <
		 sizeof ( macpath->MacAddress ) );
	memcpy ( &macpath->MacAddress, netdev->ll_addr,
		 netdev->ll_protocol->ll_addr_len );
	macpath->IfType = ntohs ( netdev->ll_protocol->ll_proto );
	if ( ( tag = vlan_tag ( netdev ) ) ) {
		vlanpath = ( ( ( void * ) macpath ) + sizeof ( *macpath ) );
		vlanpath->Header.Type = MESSAGING_DEVICE_PATH;
		vlanpath->Header.SubType = MSG_VLAN_DP;
		vlanpath->Header.Length[0] = sizeof ( *vlanpath );
		vlanpath->VlanId = tag;
		end = ( ( ( void * ) vlanpath ) + sizeof ( *vlanpath ) );
	} else {
		end = ( ( ( void * ) macpath ) + sizeof ( *macpath ) );
	}
	end->Type = END_DEVICE_PATH_TYPE;
	end->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	end->Length[0] = sizeof ( *end );

	return path;
}

/**
 * Construct EFI device path for URI
 *
 * @v uri		URI
 * @ret path		EFI device path, or NULL on error
 *
 * The caller is responsible for eventually calling free() on the
 * allocated device path.
 */
EFI_DEVICE_PATH_PROTOCOL * efi_uri_path ( struct uri *uri ) {
	EFI_DEVICE_PATH_PROTOCOL *path;
	EFI_DEVICE_PATH_PROTOCOL *end;
	URI_DEVICE_PATH *uripath;
	size_t uri_len;
	size_t uripath_len;
	size_t len;

	/* Calculate device path length */
	uri_len = ( format_uri ( uri, NULL, 0 ) + 1 /* NUL */ );
	uripath_len = ( sizeof ( *uripath ) + uri_len );
	len = ( uripath_len + sizeof ( *end ) );

	/* Allocate device path */
	path = zalloc ( len );
	if ( ! path )
		return NULL;

	/* Construct device path */
	uripath = ( ( void * ) path );
	uripath->Header.Type = MESSAGING_DEVICE_PATH;
	uripath->Header.SubType = MSG_URI_DP;
	uripath->Header.Length[0] = ( uripath_len & 0xff );
	uripath->Header.Length[1] = ( uripath_len >> 8 );
	format_uri ( uri, uripath->Uri, uri_len );
	end = ( ( ( void * ) path ) + uripath_len );
	end->Type = END_DEVICE_PATH_TYPE;
	end->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	end->Length[0] = sizeof ( *end );

	return path;
}

/**
 * Construct EFI device path for iSCSI device
 *
 * @v iscsi		iSCSI session
 * @ret path		EFI device path, or NULL on error
 */
EFI_DEVICE_PATH_PROTOCOL * efi_iscsi_path ( struct iscsi_session *iscsi ) {
	struct sockaddr_tcpip *st_target;
	struct net_device *netdev;
	EFI_DEVICE_PATH_PROTOCOL *netpath;
	EFI_DEVICE_PATH_PROTOCOL *path;
	EFI_DEVICE_PATH_PROTOCOL *end;
	ISCSI_DEVICE_PATH *iscsipath;
	char *name;
	size_t prefix_len;
	size_t name_len;
	size_t iscsi_len;
	size_t len;

	/* Get network device associated with target address */
	st_target = ( ( struct sockaddr_tcpip * ) &iscsi->target_sockaddr );
	netdev = tcpip_netdev ( st_target );
	if ( ! netdev )
		goto err_netdev;

	/* Get network device path */
	netpath = efi_netdev_path ( netdev );
	if ( ! netpath )
		goto err_netpath;

	/* Calculate device path length */
	prefix_len = efi_path_len ( netpath );
	name_len = ( strlen ( iscsi->target_iqn ) + 1 /* NUL */ );
	iscsi_len = ( sizeof ( *iscsipath ) + name_len );
	len = ( prefix_len + iscsi_len + sizeof ( *end ) );

	/* Allocate device path */
	path = zalloc ( len );
	if ( ! path )
		goto err_alloc;

	/* Construct device path */
	memcpy ( path, netpath, prefix_len );
	iscsipath = ( ( ( void * ) path ) + prefix_len );
	iscsipath->Header.Type = MESSAGING_DEVICE_PATH;
	iscsipath->Header.SubType = MSG_ISCSI_DP;
	iscsipath->Header.Length[0] = iscsi_len;
	iscsipath->LoginOption = ISCSI_LOGIN_OPTION_AUTHMETHOD_NON;
	memcpy ( &iscsipath->Lun, &iscsi->lun, sizeof ( iscsipath->Lun ) );
	name = ( ( ( void * ) iscsipath ) + sizeof ( *iscsipath ) );
	memcpy ( name, iscsi->target_iqn, name_len );
	end = ( ( ( void * ) name ) + name_len );
	end->Type = END_DEVICE_PATH_TYPE;
	end->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	end->Length[0] = sizeof ( *end );

	/* Free temporary paths */
	free ( netpath );

	return path;

 err_alloc:
	free ( netpath );
 err_netpath:
 err_netdev:
	return NULL;
}

/**
 * Construct EFI device path for AoE device
 *
 * @v aoedev		AoE device
 * @ret path		EFI device path, or NULL on error
 */
EFI_DEVICE_PATH_PROTOCOL * efi_aoe_path ( struct aoe_device *aoedev ) {
	struct {
		SATA_DEVICE_PATH sata;
		EFI_DEVICE_PATH_PROTOCOL end;
	} satapath;
	EFI_DEVICE_PATH_PROTOCOL *netpath;
	EFI_DEVICE_PATH_PROTOCOL *path;

	/* Get network device path */
	netpath = efi_netdev_path ( aoedev->netdev );
	if ( ! netpath )
		goto err_netdev;

	/* Construct SATA path */
	memset ( &satapath, 0, sizeof ( satapath ) );
	satapath.sata.Header.Type = MESSAGING_DEVICE_PATH;
	satapath.sata.Header.SubType = MSG_SATA_DP;
	satapath.sata.Header.Length[0] = sizeof ( satapath.sata );
	satapath.sata.HBAPortNumber = aoedev->major;
	satapath.sata.PortMultiplierPortNumber = aoedev->minor;
	satapath.end.Type = END_DEVICE_PATH_TYPE;
	satapath.end.SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	satapath.end.Length[0] = sizeof ( satapath.end );

	/* Construct overall device path */
	path = efi_paths ( netpath, &satapath, NULL );
	if ( ! path )
		goto err_paths;

	/* Free temporary paths */
	free ( netpath );

	return path;

 err_paths:
	free ( netpath );
 err_netdev:
	return NULL;
}

/**
 * Construct EFI device path for Fibre Channel device
 *
 * @v desc		FCP device description
 * @ret path		EFI device path, or NULL on error
 */
EFI_DEVICE_PATH_PROTOCOL * efi_fcp_path ( struct fcp_description *desc ) {
	struct {
		FIBRECHANNELEX_DEVICE_PATH fc;
		EFI_DEVICE_PATH_PROTOCOL end;
	} __attribute__ (( packed )) *path;

	/* Allocate device path */
	path = zalloc ( sizeof ( *path ) );
	if ( ! path )
		return NULL;

	/* Construct device path */
	path->fc.Header.Type = MESSAGING_DEVICE_PATH;
	path->fc.Header.SubType = MSG_FIBRECHANNELEX_DP;
	path->fc.Header.Length[0] = sizeof ( path->fc );
	memcpy ( path->fc.WWN, &desc->wwn, sizeof ( path->fc.WWN ) );
	memcpy ( path->fc.Lun, &desc->lun, sizeof ( path->fc.Lun ) );
	path->end.Type = END_DEVICE_PATH_TYPE;
	path->end.SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	path->end.Length[0] = sizeof ( path->end );

	return &path->fc.Header;
}

/**
 * Construct EFI device path for Infiniband SRP device
 *
 * @v ib_srp		Infiniband SRP device
 * @ret path		EFI device path, or NULL on error
 */
EFI_DEVICE_PATH_PROTOCOL * efi_ib_srp_path ( struct ib_srp_device *ib_srp ) {
	const struct ipxe_ib_sbft *sbft = &ib_srp->sbft;
	union ib_srp_target_port_id *id =
		container_of ( &sbft->srp.target, union ib_srp_target_port_id,
			       srp );
	struct efi_device *efidev;
	EFI_DEVICE_PATH_PROTOCOL *path;
	INFINIBAND_DEVICE_PATH *ibpath;
	EFI_DEVICE_PATH_PROTOCOL *end;
	size_t prefix_len;
	size_t len;

	/* Find parent EFI device */
	efidev = efidev_parent ( ib_srp->ibdev->dev );
	if ( ! efidev )
		return NULL;

	/* Calculate device path length */
	prefix_len = efi_path_len ( efidev->path );
	len = ( prefix_len + sizeof ( *ibpath ) + sizeof ( *end ) );

	/* Allocate device path */
	path = zalloc ( len );
	if ( ! path )
		return NULL;

	/* Construct device path */
	memcpy ( path, efidev->path, prefix_len );
	ibpath = ( ( ( void * ) path ) + prefix_len );
	ibpath->Header.Type = MESSAGING_DEVICE_PATH;
	ibpath->Header.SubType = MSG_INFINIBAND_DP;
	ibpath->Header.Length[0] = sizeof ( *ibpath );
	ibpath->ResourceFlags = INFINIBAND_RESOURCE_FLAG_STORAGE_PROTOCOL;
	memcpy ( ibpath->PortGid, &sbft->ib.dgid, sizeof ( ibpath->PortGid ) );
	memcpy ( &ibpath->ServiceId, &sbft->ib.service_id,
		 sizeof ( ibpath->ServiceId ) );
	memcpy ( &ibpath->TargetPortId, &id->ib.ioc_guid,
		 sizeof ( ibpath->TargetPortId ) );
	memcpy ( &ibpath->DeviceId, &id->ib.id_ext,
		 sizeof ( ibpath->DeviceId ) );
	end = ( ( ( void * ) ibpath ) + sizeof ( *ibpath ) );
	end->Type = END_DEVICE_PATH_TYPE;
	end->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	end->Length[0] = sizeof ( *end );

	return path;
}

/**
 * Construct EFI device path for USB function
 *
 * @v func		USB function
 * @ret path		EFI device path, or NULL on error
 *
 * The caller is responsible for eventually calling free() on the
 * allocated device path.
 */
EFI_DEVICE_PATH_PROTOCOL * efi_usb_path ( struct usb_function *func ) {
	struct usb_device *usb = func->usb;
	struct efi_device *efidev;
	EFI_DEVICE_PATH_PROTOCOL *path;
	EFI_DEVICE_PATH_PROTOCOL *end;
	USB_DEVICE_PATH *usbpath;
	unsigned int count;
	size_t prefix_len;
	size_t len;

	/* Sanity check */
	assert ( func->desc.count >= 1 );

	/* Find parent EFI device */
	efidev = efidev_parent ( &func->dev );
	if ( ! efidev )
		return NULL;

	/* Calculate device path length */
	count = ( usb_depth ( usb ) + 1 );
	prefix_len = efi_path_len ( efidev->path );
	len = ( prefix_len + ( count * sizeof ( *usbpath ) ) +
		sizeof ( *end ) );

	/* Allocate device path */
	path = zalloc ( len );
	if ( ! path )
		return NULL;

	/* Construct device path */
	memcpy ( path, efidev->path, prefix_len );
	end = ( ( ( void * ) path ) + len - sizeof ( *end ) );
	end->Type = END_DEVICE_PATH_TYPE;
	end->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	end->Length[0] = sizeof ( *end );
	usbpath = ( ( ( void * ) end ) - sizeof ( *usbpath ) );
	usbpath->InterfaceNumber = func->interface[0];
	for ( ; usb ; usbpath--, usb = usb->port->hub->usb ) {
		usbpath->Header.Type = MESSAGING_DEVICE_PATH;
		usbpath->Header.SubType = MSG_USB_DP;
		usbpath->Header.Length[0] = sizeof ( *usbpath );
		usbpath->ParentPortNumber = ( usb->port->address - 1 );
	}

	return path;
}

/**
 * Describe object as an EFI device path
 *
 * @v intf		Interface
 * @ret path		EFI device path, or NULL
 *
 * The caller is responsible for eventually calling free() on the
 * allocated device path.
 */
EFI_DEVICE_PATH_PROTOCOL * efi_describe ( struct interface *intf ) {
	struct interface *dest;
	efi_describe_TYPE ( void * ) *op =
		intf_get_dest_op ( intf, efi_describe, &dest );
	void *object = intf_object ( dest );
	EFI_DEVICE_PATH_PROTOCOL *path;

	if ( op ) {
		path = op ( object );
	} else {
		path = NULL;
	}

	intf_put ( dest );
	return path;
}
