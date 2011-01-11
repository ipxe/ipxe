/*
 * Copyright (C) 2010 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdio.h>
#include <ipxe/nvs.h>
#include <ipxe/pci.h>
#include <ipxe/pcivpd.h>
#include <ipxe/nvsvpd.h>

/** @file
 *
 * Non-Volatile Storage using Vital Product Data
 *
 */

/**
 * Read from VPD
 *
 * @v nvs		NVS device
 * @v address		Starting address
 * @v buf		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
static int nvs_vpd_read ( struct nvs_device *nvs, unsigned int address,
			  void *data, size_t len ) {
	struct nvs_vpd_device *nvsvpd =
		container_of ( nvs, struct nvs_vpd_device, nvs );
	int rc;

	if ( ( rc = pci_vpd_read ( &nvsvpd->vpd, ( nvsvpd->address + address ),
				   data, len ) ) != 0 ) {
		DBGC ( nvsvpd->vpd.pci, PCI_FMT " NVS could not read "
		       "[%04x,%04zx): %s\n", PCI_ARGS ( nvsvpd->vpd.pci ),
		       address, ( address + len ), strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Write to VPD
 *
 * @v nvs		NVS device
 * @v address		Starting address
 * @v buf		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
static int nvs_vpd_write ( struct nvs_device *nvs, unsigned int address,
			   const void *data, size_t len ) {
	struct nvs_vpd_device *nvsvpd =
		container_of ( nvs, struct nvs_vpd_device, nvs );
	int rc;

	if ( ( rc = pci_vpd_write ( &nvsvpd->vpd, ( nvsvpd->address + address ),
				    data, len ) ) != 0 ) {
		DBGC ( nvsvpd->vpd.pci, PCI_FMT " NVS could not write "
		       "[%04x,%04zx): %s\n", PCI_ARGS ( nvsvpd->vpd.pci ),
		       address, ( address + len ), strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Initialise NVS VPD device
 *
 * @v nvsvpd		NVS VPD device
 * @v pci		PCI device
 * @ret rc		Return status code
 */
int nvs_vpd_init ( struct nvs_vpd_device *nvsvpd, struct pci_device *pci,
		   unsigned int field ) {
	size_t len;
	int rc;

	/* Initialise VPD device */
	if ( ( rc = pci_vpd_init ( &nvsvpd->vpd, pci ) ) != 0 ) {
		DBGC ( pci, PCI_FMT " NVS could not initialise "
		       "VPD: %s\n", PCI_ARGS ( pci ), strerror ( rc ) );
		return rc;
	}

	/* Locate VPD field */
	if ( ( rc = pci_vpd_find ( &nvsvpd->vpd, field, &nvsvpd->address,
				   &len ) ) != 0 ) {
		DBGC ( pci, PCI_FMT " NVS could not locate VPD field "
		       PCI_VPD_FIELD_FMT ": %s\n", PCI_ARGS ( pci ),
		       PCI_VPD_FIELD_ARGS ( field ), strerror ( rc ) );
		return rc;
	}

	/* Initialise NVS device */
	nvsvpd->nvs.size = len;
	nvsvpd->nvs.read = nvs_vpd_read;
	nvsvpd->nvs.write = nvs_vpd_write;

	DBGC ( pci, PCI_FMT " NVS using VPD field " PCI_VPD_FIELD_FMT " at "
	       "[%04x,%04x)\n", PCI_ARGS ( pci ), PCI_VPD_FIELD_ARGS ( field ),
	       nvsvpd->address, ( nvsvpd->address + nvsvpd->nvs.size ) );

	return 0;
}
