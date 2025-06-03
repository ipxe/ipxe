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

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/netdevice.h>
#include <ipxe/vlan.h>
#include <ipxe/uuid.h>
#include <ipxe/tcpip.h>
#include <ipxe/uri.h>
#include <ipxe/iscsi.h>
#include <ipxe/aoe.h>
#include <ipxe/fcp.h>
#include <ipxe/ib_srp.h>
#include <ipxe/usb.h>
#include <ipxe/settings.h>
#include <ipxe/dhcp.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_path.h>

/** @file
 *
 * EFI device paths
 *
 */

/** An EFI device path settings block */
struct efi_path_settings {
	/** Settings interface */
	struct settings settings;
	/** Device path */
	EFI_DEVICE_PATH_PROTOCOL *path;
};

/** An EFI device path setting */
struct efi_path_setting {
	/** Setting */
	const struct setting *setting;
	/**
	 * Fetch setting
	 *
	 * @v pathset		Path setting
	 * @v path		Device path
	 * @v data		Buffer to fill with setting data
	 * @v len		Length of buffer
	 * @ret len		Length of setting data, or negative error
	 */
	int ( * fetch ) ( struct efi_path_setting *pathset,
			  EFI_DEVICE_PATH_PROTOCOL *path,
			  void *data, size_t len );
	/** Path type */
	uint8_t type;
	/** Path subtype */
	uint8_t subtype;
	/** Offset within device path */
	uint8_t offset;
	/** Length (if fixed) */
	uint8_t len;
};

/**
 * Find next element in device path
 *
 * @v path		Device path, or NULL
 * @v next		Next element in device path, or NULL if at end
 */
EFI_DEVICE_PATH_PROTOCOL * efi_path_next ( EFI_DEVICE_PATH_PROTOCOL *path ) {

	/* Check for non-existent device path */
	if ( ! path )
		return NULL;

	/* Check for end of device path */
	if ( path->Type == END_DEVICE_PATH_TYPE )
		return NULL;

	/* Move to next component of the device path */
	path = ( ( ( void * ) path ) +
		 /* There's this amazing new-fangled thing known as
		  * a UINT16, but who wants to use one of those? */
		 ( ( path->Length[1] << 8 ) | path->Length[0] ) );

	return path;
}

/**
 * Find previous element of device path
 *
 * @v path		Device path, or NULL for no path
 * @v curr		Current element in device path, or NULL for end of path
 * @ret prev		Previous element in device path, or NULL
 */
EFI_DEVICE_PATH_PROTOCOL * efi_path_prev ( EFI_DEVICE_PATH_PROTOCOL *path,
					   EFI_DEVICE_PATH_PROTOCOL *curr ) {
	EFI_DEVICE_PATH_PROTOCOL *tmp;

	/* Find immediately preceding element */
	while ( ( tmp = efi_path_next ( path ) ) != curr ) {
		path = tmp;
	}

	return path;
}

/**
 * Find end of device path
 *
 * @v path		Device path, or NULL
 * @ret path_end	End of device path, or NULL
 */
EFI_DEVICE_PATH_PROTOCOL * efi_path_end ( EFI_DEVICE_PATH_PROTOCOL *path ) {

	return efi_path_prev ( path, NULL );
}

/**
 * Find length of device path (excluding terminator)
 *
 * @v path		Device path, or NULL
 * @ret path_len	Length of device path
 */
size_t efi_path_len ( EFI_DEVICE_PATH_PROTOCOL *path ) {
	EFI_DEVICE_PATH_PROTOCOL *end = efi_path_end ( path );

	return ( ( ( void * ) end ) - ( ( void * ) path ) );
}

/**
 * Get MAC address from device path
 *
 * @v path		Device path
 * @ret mac		MAC address, or NULL if not found
 */
void * efi_path_mac ( EFI_DEVICE_PATH_PROTOCOL *path ) {
	EFI_DEVICE_PATH_PROTOCOL *next;
	MAC_ADDR_DEVICE_PATH *mac;

	/* Search for MAC address path */
	for ( ; ( next = efi_path_next ( path ) ) ; path = next ) {
		if ( ( path->Type == MESSAGING_DEVICE_PATH ) &&
		     ( path->SubType == MSG_MAC_ADDR_DP ) ) {
			mac = container_of ( path, MAC_ADDR_DEVICE_PATH,
					     Header );
			return &mac->MacAddress;
		}
	}

	/* No MAC address found */
	return NULL;
}

/**
 * Get VLAN tag from device path
 *
 * @v path		Device path
 * @ret tag		VLAN tag, or 0 if not a VLAN
 */
unsigned int efi_path_vlan ( EFI_DEVICE_PATH_PROTOCOL *path ) {
	EFI_DEVICE_PATH_PROTOCOL *next;
	VLAN_DEVICE_PATH *vlan;

	/* Search for VLAN device path */
	for ( ; ( next = efi_path_next ( path ) ) ; path = next ) {
		if ( ( path->Type == MESSAGING_DEVICE_PATH ) &&
		     ( path->SubType == MSG_VLAN_DP ) ) {
			vlan = container_of ( path, VLAN_DEVICE_PATH, Header );
			return vlan->VlanId;
		}
	}

	/* No VLAN device path found */
	return 0;
}

/**
 * Get partition GUID from device path
 *
 * @v path		Device path
 * @v guid		Partition GUID to fill in
 * @ret rc		Return status code
 */
int efi_path_guid ( EFI_DEVICE_PATH_PROTOCOL *path, union uuid *guid ) {
	EFI_DEVICE_PATH_PROTOCOL *next;
	HARDDRIVE_DEVICE_PATH *hd;
	int rc;

	/* Search for most specific partition device path */
	rc = -ENOENT;
	for ( ; ( next = efi_path_next ( path ) ) ; path = next ) {

		/* Skip non-harddrive device paths */
		if ( path->Type != MEDIA_DEVICE_PATH )
			continue;
		if ( path->SubType != MEDIA_HARDDRIVE_DP )
			continue;

		/* Skip non-GUID signatures */
		hd = container_of ( path, HARDDRIVE_DEVICE_PATH, Header );
		if ( hd->SignatureType != SIGNATURE_TYPE_GUID )
			continue;

		/* Extract GUID */
		memcpy ( guid, hd->Signature, sizeof ( *guid ) );
		uuid_mangle ( guid );

		/* Record success, but continue searching in case
		 * there exists a more specific GUID (e.g. a partition
		 * GUID rather than a disk GUID).
		 */
		rc = 0;
	}

	return rc;
}

/**
 * Parse URI from device path
 *
 * @v path		Device path
 * @ret uri		URI, or NULL if not a URI
 */
struct uri * efi_path_uri ( EFI_DEVICE_PATH_PROTOCOL *path ) {
	EFI_DEVICE_PATH_PROTOCOL *next;
	URI_DEVICE_PATH *uripath;
	char *uristring;
	struct uri *uri;
	size_t len;

	/* Search for URI device path */
	for ( ; ( next = efi_path_next ( path ) ) ; path = next ) {
		if ( ( path->Type == MESSAGING_DEVICE_PATH ) &&
		     ( path->SubType == MSG_URI_DP ) ) {

			/* Calculate path length */
			uripath = container_of ( path, URI_DEVICE_PATH,
						 Header );
			len = ( ( ( path->Length[1] << 8 ) | path->Length[0] )
				- offsetof ( typeof ( *uripath ), Uri ) );

			/* Parse URI */
			uristring = zalloc ( len + 1 /* NUL */ );
			if ( ! uristring )
				return NULL;
			memcpy ( uristring, uripath->Uri, len );
			uri = parse_uri ( uristring );
			free ( uristring );

			return uri;
		}
	}

	/* No URI path found */
	return NULL;
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
	efi_path_terminate ( end );

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
	efi_path_terminate ( end );

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
	efi_path_terminate ( end );

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
	efi_path_terminate ( end );

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
	efi_path_terminate ( &satapath.end );

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
	efi_path_terminate ( &path->end );

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
	efi_path_terminate ( end );

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
	efi_path_terminate ( end );
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

/**
 * Fetch an EFI device path fixed-size setting
 *
 * @v pathset		Path setting
 * @v path		Device path
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int efi_path_fetch_fixed ( struct efi_path_setting *pathset,
				  EFI_DEVICE_PATH_PROTOCOL *path,
				  void *data, size_t len ) {

	/* Copy data */
	if ( len > pathset->len )
		len = pathset->len;
	memcpy ( data, ( ( ( void * ) path ) + pathset->offset ), len );

	return pathset->len;
}

/**
 * Fetch an EFI device path DNS setting
 *
 * @v pathset		Path setting
 * @v path		Device path
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int efi_path_fetch_dns ( struct efi_path_setting *pathset,
				EFI_DEVICE_PATH_PROTOCOL *path,
				void *data, size_t len ) {
	DNS_DEVICE_PATH *dns = container_of ( path, DNS_DEVICE_PATH, Header );
	unsigned int count;
	unsigned int i;
	size_t frag_len;

	/* Check applicability */
	if ( ( !! dns->IsIPv6 ) !=
	     ( pathset->setting->type == &setting_type_ipv6 ) )
		return -ENOENT;

	/* Calculate number of addresses */
	count = ( ( ( ( path->Length[1] << 8 ) | path->Length[0] ) -
		    pathset->offset ) / sizeof ( dns->DnsServerIp[0] ) );

	/* Copy data */
	for ( i = 0 ; i < count ; i++ ) {
		frag_len = len;
		if ( frag_len > pathset->len )
			frag_len = pathset->len;
		memcpy ( data, &dns->DnsServerIp[i], frag_len );
		data += frag_len;
		len -= frag_len;
	}

	return ( count * pathset->len );
}

/** EFI device path settings */
static struct efi_path_setting efi_path_settings[] = {
	{ &ip_setting, efi_path_fetch_fixed, MESSAGING_DEVICE_PATH,
	  MSG_IPv4_DP, offsetof ( IPv4_DEVICE_PATH, LocalIpAddress ),
	  sizeof ( struct in_addr ) },
	{ &netmask_setting, efi_path_fetch_fixed, MESSAGING_DEVICE_PATH,
	  MSG_IPv4_DP, offsetof ( IPv4_DEVICE_PATH, SubnetMask ),
	  sizeof ( struct in_addr ) },
	{ &gateway_setting, efi_path_fetch_fixed, MESSAGING_DEVICE_PATH,
	  MSG_IPv4_DP, offsetof ( IPv4_DEVICE_PATH, GatewayIpAddress ),
	  sizeof ( struct in_addr ) },
	{ &ip6_setting, efi_path_fetch_fixed, MESSAGING_DEVICE_PATH,
	  MSG_IPv6_DP, offsetof ( IPv6_DEVICE_PATH, LocalIpAddress ),
	  sizeof ( struct in6_addr ) },
	{ &len6_setting, efi_path_fetch_fixed, MESSAGING_DEVICE_PATH,
	  MSG_IPv6_DP, offsetof ( IPv6_DEVICE_PATH, PrefixLength ),
	  sizeof ( uint8_t ) },
	{ &gateway6_setting, efi_path_fetch_fixed, MESSAGING_DEVICE_PATH,
	  MSG_IPv6_DP, offsetof ( IPv6_DEVICE_PATH, GatewayIpAddress ),
	  sizeof ( struct in6_addr ) },
	{ &dns_setting, efi_path_fetch_dns, MESSAGING_DEVICE_PATH,
	  MSG_DNS_DP, offsetof ( DNS_DEVICE_PATH, DnsServerIp ),
	  sizeof ( struct in_addr ) },
	{ &dns6_setting, efi_path_fetch_dns, MESSAGING_DEVICE_PATH,
	  MSG_DNS_DP, offsetof ( DNS_DEVICE_PATH, DnsServerIp ),
	  sizeof ( struct in6_addr ) },
};

/**
 * Fetch value of EFI device path setting
 *
 * @v settings		Settings block
 * @v setting		Setting to fetch
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int efi_path_fetch ( struct settings *settings, struct setting *setting,
			    void *data, size_t len ) {
	struct efi_path_settings *pathsets =
		container_of ( settings, struct efi_path_settings, settings );
	EFI_DEVICE_PATH_PROTOCOL *path = pathsets->path;
	EFI_DEVICE_PATH_PROTOCOL *next;
	struct efi_path_setting *pathset;
	unsigned int i;
	int ret;

	/* Find matching path setting, if any */
	for ( i = 0 ; i < ( sizeof ( efi_path_settings ) /
			    sizeof ( efi_path_settings[0] ) ) ; i++ ) {

		/* Check for a matching setting */
		pathset = &efi_path_settings[i];
		if ( setting_cmp ( setting, pathset->setting ) != 0 )
			continue;

		/* Find matching device path element, if any */
		for ( ; ( next = efi_path_next ( path ) ) ; path = next ) {

			/* Check for a matching path type */
			if ( ( path->Type != pathset->type ) ||
			     ( path->SubType != pathset->subtype ) )
				continue;

			/* Fetch value */
			if ( ( ret = pathset->fetch ( pathset, path,
						      data, len ) ) < 0 )
				return ret;

			/* Apply default type, if not already set */
			if ( ! setting->type )
				setting->type = pathset->setting->type;

			return ret;
		}
		break;
	}

	return -ENOENT;
}

/** EFI device path settings operations */
static struct settings_operations efi_path_settings_operations = {
	.fetch = efi_path_fetch,
};

/**
 * Create per-netdevice EFI path settings
 *
 * @v netdev		Network device
 * @v priv		Private data
 * @ret rc		Return status code
 */
static int efi_path_net_probe ( struct net_device *netdev, void *priv ) {
	struct efi_path_settings *pathsets = priv;
	struct settings *settings = &pathsets->settings;
	EFI_DEVICE_PATH_PROTOCOL *path = efi_loaded_image_path;
	unsigned int vlan;
	void *mac;
	int rc;

	/* Check applicability */
	pathsets->path = path;
	mac = efi_path_mac ( path );
	vlan = efi_path_vlan ( path );
	if ( ( mac == NULL ) ||
	     ( memcmp ( mac, netdev->ll_addr,
			netdev->ll_protocol->ll_addr_len ) != 0 ) ||
	     ( vlan != vlan_tag ( netdev ) ) ) {
		DBGC ( settings, "EFI path %s does not apply to %s\n",
		       efi_devpath_text ( path ), netdev->name );
		return 0;
	}

	/* Never override a real DHCP settings block */
	if ( find_child_settings ( netdev_settings ( netdev ),
				   DHCP_SETTINGS_NAME ) ) {
		DBGC ( settings, "EFI path %s not overriding %s DHCP "
		       "settings\n", efi_devpath_text ( path ), netdev->name );
		return 0;
	}

	/* Initialise and register settings */
	settings_init ( settings, &efi_path_settings_operations,
			&netdev->refcnt, NULL );
	if ( ( rc = register_settings ( settings, netdev_settings ( netdev ),
					DHCP_SETTINGS_NAME ) ) != 0 ) {
		DBGC ( settings, "EFI path %s could not register for %s: %s\n",
		       efi_devpath_text ( path ), netdev->name,
		       strerror ( rc ) );
		return rc;
	}
	DBGC ( settings, "EFI path %s registered for %s\n",
	       efi_devpath_text ( path ), netdev->name );

	return 0;
}

/** EFI path settings per-netdevice driver */
struct net_driver efi_path_net_driver __net_driver = {
	.name = "EFI path",
	.priv_len = sizeof ( struct efi_path_settings ),
	.probe = efi_path_net_probe,
};
