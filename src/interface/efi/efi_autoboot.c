/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <ipxe/if_ether.h>
#include <ipxe/vlan.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_path.h>
#include <ipxe/efi/efi_autoboot.h>
#include <ipxe/efi/Protocol/SimpleNetwork.h>
#include <usr/autoboot.h>

/** @file
 *
 * EFI autoboot device
 *
 */

/**
 * Identify autoboot device
 *
 * @v device		Device handle
 * @v path		Device path
 * @ret rc		Return status code
 */
int efi_set_autoboot_ll_addr ( EFI_HANDLE device,
			       EFI_DEVICE_PATH_PROTOCOL *path ) {
	EFI_SIMPLE_NETWORK_PROTOCOL *snp;
	EFI_SIMPLE_NETWORK_MODE *mode;
	unsigned int vlan;
	int rc;

	/* Look for an SNP instance on the image's device handle */
	if ( ( rc = efi_open ( device, &efi_simple_network_protocol_guid,
			       &snp ) ) != 0 ) {
		DBGC ( device, "EFI %s has no SNP instance: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}

	/* Record autoboot device */
	mode = snp->Mode;
	vlan = efi_path_vlan ( path );
	set_autoboot_ll_addr ( &mode->CurrentAddress, mode->HwAddressSize,
			       vlan );
	DBGC ( device, "EFI %s found autoboot link-layer address:\n",
	       efi_handle_name ( device ) );
	DBGC_HDA ( device, 0, &mode->CurrentAddress, mode->HwAddressSize );
	if ( vlan ) {
		DBGC ( device, "EFI %s found autoboot VLAN %d\n",
		       efi_handle_name ( device ), vlan );
	}

	/* Configure automatic VLAN device, if applicable */
	if ( vlan && ( mode->HwAddressSize == ETH_ALEN ) ) {
		vlan_auto ( &mode->CurrentAddress, vlan );
		DBGC ( device, "EFI %s configured automatic VLAN %d\n",
		       efi_handle_name ( device ), vlan );
	}

	return 0;
}
