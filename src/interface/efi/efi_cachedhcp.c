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
#include <ipxe/cachedhcp.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_path.h>
#include <ipxe/efi/efi_cachedhcp.h>
#include <ipxe/efi/Protocol/PxeBaseCode.h>

/** @file
 *
 * EFI cached DHCP packet
 *
 */

/**
 * Record cached DHCP packet
 *
 * @v device		Device handle
 * @v path		Device path
 * @ret rc		Return status code
 */
int efi_cachedhcp_record ( EFI_HANDLE device,
			   EFI_DEVICE_PATH_PROTOCOL *path ) {
	unsigned int vlan;
	EFI_PXE_BASE_CODE_PROTOCOL *pxe;
	EFI_PXE_BASE_CODE_MODE *mode;
	int rc;

	/* Get VLAN tag, if any */
	vlan = efi_path_vlan ( path );

	/* Look for a PXE base code instance on the image's device handle */
	if ( ( rc = efi_open ( device, &efi_pxe_base_code_protocol_guid,
			       &pxe ) ) != 0 ) {
		DBGC ( device, "EFI %s has no PXE base code instance: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}

	/* Do not attempt to cache IPv6 packets */
	mode = pxe->Mode;
	if ( mode->UsingIpv6 ) {
		DBGC ( device, "EFI %s has IPv6 PXE base code\n",
		       efi_handle_name ( device ) );
		return -ENOTSUP;
	}

	/* Record DHCPACK, if present */
	if ( mode->DhcpAckReceived &&
	     ( ( rc = cachedhcp_record ( &cached_dhcpack, vlan, &mode->DhcpAck,
					 sizeof ( mode->DhcpAck ) ) ) != 0 ) ) {
		DBGC ( device, "EFI %s could not record DHCPACK: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}

	/* Record ProxyDHCPOFFER, if present */
	if ( mode->ProxyOfferReceived &&
	     ( ( rc = cachedhcp_record ( &cached_proxydhcp, vlan,
					 &mode->ProxyOffer,
					 sizeof ( mode->ProxyOffer ) ) ) != 0)){
		DBGC ( device, "EFI %s could not record ProxyDHCPOFFER: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}

	/* Record PxeBSACK, if present */
	if ( mode->PxeReplyReceived &&
	     ( ( rc = cachedhcp_record ( &cached_pxebs, vlan, &mode->PxeReply,
					 sizeof ( mode->PxeReply ) ) ) != 0 )){
		DBGC ( device, "EFI %s could not record PXEBSACK: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}

	return 0;
}
