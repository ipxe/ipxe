/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <errno.h>
#include <gpxe/pci.h>
#include <gpxe/efi/efi.h>
#include <gpxe/efi/Protocol/PciRootBridgeIo.h>

/** @file
 *
 * gPXE PCI I/O API for EFI
 *
 */

/** PCI root bridge I/O protocol */
static EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *efipci;
EFI_REQUIRE_PROTOCOL ( EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL, &efipci );

static unsigned long efipci_address ( struct pci_device *pci,
				      unsigned long location ) {
	return EFI_PCI_ADDRESS ( pci->bus, PCI_SLOT ( pci->devfn ),
				 PCI_FUNC ( pci->devfn ),
				 EFIPCI_OFFSET ( location ) );
}

int efipci_read ( struct pci_device *pci, unsigned long location,
		  void *value ) {
	EFI_STATUS efirc;

	if ( ( efirc = efipci->Pci.Read ( efipci, EFIPCI_WIDTH ( location ),
					  efipci_address ( pci, location ), 1,
					  value ) ) != 0 ) {
		DBG ( "EFIPCI config read from %02x:%02x.%x offset %02lx "
		      "failed: %s\n", pci->bus, PCI_SLOT ( pci->devfn ),
		      PCI_FUNC ( pci->devfn ), EFIPCI_OFFSET ( location ),
		      efi_strerror ( efirc ) );
		return -EIO;
	}

	return 0;
}

int efipci_write ( struct pci_device *pci, unsigned long location,
		   unsigned long value ) {
	EFI_STATUS efirc;

	if ( ( efirc = efipci->Pci.Write ( efipci, EFIPCI_WIDTH ( location ),
					   efipci_address ( pci, location ), 1,
					   &value ) ) != 0 ) {
		DBG ( "EFIPCI config write to %02x:%02x.%x offset %02lx "
		      "failed: %s\n", pci->bus, PCI_SLOT ( pci->devfn ),
		      PCI_FUNC ( pci->devfn ), EFIPCI_OFFSET ( location ),
		      efi_strerror ( efirc ) );
		return -EIO;
	}

	return 0;
}

PROVIDE_PCIAPI_INLINE ( efi, pci_max_bus );
PROVIDE_PCIAPI_INLINE ( efi, pci_read_config_byte );
PROVIDE_PCIAPI_INLINE ( efi, pci_read_config_word );
PROVIDE_PCIAPI_INLINE ( efi, pci_read_config_dword );
PROVIDE_PCIAPI_INLINE ( efi, pci_write_config_byte );
PROVIDE_PCIAPI_INLINE ( efi, pci_write_config_word );
PROVIDE_PCIAPI_INLINE ( efi, pci_write_config_dword );
