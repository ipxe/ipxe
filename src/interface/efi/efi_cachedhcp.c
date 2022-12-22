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
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	unsigned int vlan;
	union {
		EFI_PXE_BASE_CODE_PROTOCOL *pxe;
		void *interface;
	} pxe;
	EFI_PXE_BASE_CODE_MODE *mode;
	EFI_STATUS efirc;
	int rc;

	/* Get VLAN tag, if any */
	vlan = efi_path_vlan ( path );

	/* Look for a PXE base code instance on the image's device handle */
	if ( ( efirc = bs->OpenProtocol ( device,
					  &efi_pxe_base_code_protocol_guid,
					  &pxe.interface, efi_image_handle,
					  NULL,
					  EFI_OPEN_PROTOCOL_GET_PROTOCOL ))!=0){
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s has no PXE base code instance: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_open;
	}

	/* Do not attempt to cache IPv6 packets */
	mode = pxe.pxe->Mode;
	if ( mode->UsingIpv6 ) {
		rc = -ENOTSUP;
		DBGC ( device, "EFI %s has IPv6 PXE base code\n",
		       efi_handle_name ( device ) );
		goto err_ipv6;
	}

	/* Record DHCPACK, if present */
	if ( mode->DhcpAckReceived &&
	     ( ( rc = cachedhcp_record ( &cached_dhcpack, vlan,
					 virt_to_user ( &mode->DhcpAck ),
					 sizeof ( mode->DhcpAck ) ) ) != 0 ) ) {
		DBGC ( device, "EFI %s could not record DHCPACK: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_dhcpack;
	}

	/* Record ProxyDHCPOFFER, if present */
	if ( mode->ProxyOfferReceived &&
	     ( ( rc = cachedhcp_record ( &cached_proxydhcp, vlan,
					 virt_to_user ( &mode->ProxyOffer ),
					 sizeof ( mode->ProxyOffer ) ) ) != 0)){
		DBGC ( device, "EFI %s could not record ProxyDHCPOFFER: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_proxydhcp;
	}

	/* Record PxeBSACK, if present */
	if ( mode->PxeReplyReceived &&
	     ( ( rc = cachedhcp_record ( &cached_pxebs, vlan,
					 virt_to_user ( &mode->PxeReply ),
					 sizeof ( mode->PxeReply ) ) ) != 0)){
		DBGC ( device, "EFI %s could not record PXEBSACK: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_pxebs;
	}

	/* Success */
	rc = 0;

 err_pxebs:
 err_proxydhcp:
 err_dhcpack:
 err_ipv6:
	bs->CloseProtocol ( device, &efi_pxe_base_code_protocol_guid,
			    efi_image_handle, NULL );
 err_open:
	return rc;
}
