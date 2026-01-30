/*
 * Copyright (C) 2026 Jaromir Capik <jaromir.capik@email.cz>.
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

#include <stdint.h>
#include <ipxe/pci.h>

/** @file
 *
 * PCI BIOS guard
 *
 */

/**
 * Signal whether it's safe to call PCI BIOS
 *
 * @ret True	PCI bus found - it is safe to call PCI BIOS
 * @ret False	PCI bus not found - it is NOT safe to call PCI BIOS
 */
int pcibios_guard_pcibios_safe ( void ) {
	uint32_t backup;
	static int pci_bus_found = 0;

	if ( ! pci_bus_found ) {
		/* Method 1 of PCI bus detection */
		outb ( 0x01, 0xCFB );
		backup = inl ( 0xCF8 );
		outl ( 0x80000000, 0xCF8 );
		pci_bus_found = ( inl ( 0xCF8 ) == 0x80000000 );
		outl ( backup, 0xCF8 );
	}

	if ( ! pci_bus_found ) {
		/* Method 2 of PCI bus detection */
		outb ( 0x00, 0xCFB );
		outb ( 0x00, 0xCF8 );
		outb ( 0x00, 0xCFA );
		pci_bus_found = ( ( ! inb ( 0xCF8 ) ) && ( ! inb (0xCFA) ) );
	}

	return pci_bus_found;
}

#ifdef PCIBIOS_GUARD
PROVIDE_PCIAPI ( pcbios, pcibios_safe, pcibios_guard_pcibios_safe );
#endif
