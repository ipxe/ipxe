/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <gpxe/pci.h>
#include <pcibios.h>
#include <realmode.h>

/** @file
 *
 * PCI configuration space access via PCI BIOS
 *
 */

/**
 * Determine maximum PCI bus number within system
 *
 * @ret max_bus		Maximum bus number
 */
int pcibios_max_bus ( void ) {
	int discard_a;
	uint8_t max_bus;

	REAL_EXEC ( rm_pcibios_check,
		    "stc\n\t"
		    "int $0x1a\n\t"
		    "jnc 1f\n\t"
		    "xorw %%cx, %%cx\n\t"
		    "\n1:\n\t",
		    2,
		    OUT_CONSTRAINTS ( "=a" ( discard_a ), "=c" ( max_bus ) ),
		    IN_CONSTRAINTS ( "a" ( PCIBIOS_INSTALLATION_CHECK >> 16 )),
		    CLOBBER ( "ebx", "edx", "edi" ) );
	
	return max_bus;
}

/**
 * Read configuration space via PCI BIOS
 *
 * @v pci	PCI device
 * @v command	PCI BIOS command
 * @v value	Value read
 * @ret rc	Return status code
 */
int pcibios_read ( struct pci_device *pci, uint32_t command, uint32_t *value ){
	int discard_b, discard_D;
	int status;

	REAL_EXEC ( rm_pcibios_read,
		    "stc\n\t"
		    "int $0x1a\n\t"
		    "jnc 1f\n\t"
		    "xorl %%eax, %%eax\n\t"
		    "decl %%eax\n\t"
		    "movl %%eax, %%ecx\n\t"
		    "\n1:\n\t",
		    4,
		    OUT_CONSTRAINTS ( "=a" ( status ), "=b" ( discard_b ),
				      "=c" ( *value ), "=D" ( discard_D ) ),
		    IN_CONSTRAINTS ( "a" ( command >> 16 ),
				     "b" ( ( pci->bus << 8 ) | pci->devfn ),
				     "D" ( command ) ),
		    CLOBBER ( "edx" ) );
	
	return ( ( status >> 8 ) & 0xff );
}

/**
 * Write configuration space via PCI BIOS
 *
 * @v pci	PCI device
 * @v command	PCI BIOS command
 * @v value	Value to be written
 * @ret rc	Return status code
 */
int pcibios_write ( struct pci_device *pci, uint32_t command, uint32_t value ){
	int discard_b, discard_c, discard_D;
	int status;

	REAL_EXEC ( rm_pcibios_write,
		    "stc\n\t"
		    "int $0x1a\n\t"
		    "jnc 1f\n\t"
		    "movb $0xff, %%ah\n\t"
		    "\n1:\n\t",
		    4,
		    OUT_CONSTRAINTS ( "=a" ( status ), "=b" ( discard_b ),
				      "=c" ( discard_c ), "=D" ( discard_D ) ),
		    IN_CONSTRAINTS ( "a" ( command >> 16 ),
				     "b" ( ( pci->bus << 8 ) | pci->devfn ),
				     "c" ( value ), "D" ( command ) ),
		    CLOBBER ( "edx" ) );
	
	return ( ( status >> 8 ) & 0xff );
}
