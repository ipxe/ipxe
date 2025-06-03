/*
 * Copyright (C) 2022 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#include <ipxe/ecam.h>

/** @file
 *
 * PCI Enhanced Configuration Access Mechanism (ECAM)
 *
 */

/** Cached mapped ECAM allocation */
static struct ecam_mapping ecam;

/**
 * Find lowest ECAM allocation not below a given PCI bus:dev.fn address
 *
 * @v busdevfn		PCI bus:dev.fn address
 * @v range		PCI device address range to fill in
 * @v alloc		ECAM allocation to fill in, or NULL
 * @ret rc		Return status code
 */
static int ecam_find ( uint32_t busdevfn, struct pci_range *range,
		       struct ecam_allocation *alloc ) {
	struct ecam_table *mcfg;
	struct ecam_allocation *tmp;
	unsigned int best = 0;
	unsigned int count;
	unsigned int index;
	unsigned int i;
	uint32_t start;

	/* Return empty range on error */
	range->count = 0;

	/* Locate MCFG table */
	mcfg = container_of ( acpi_table ( ECAM_SIGNATURE, 0 ),
			      struct ecam_table, acpi );
	if ( ! mcfg ) {
		DBGC ( &ecam, "ECAM found no MCFG table\n" );
		return -ENOTSUP;
	}

	/* Iterate over allocations */
	for ( i = 0 ; ( offsetof ( typeof ( *mcfg ), alloc[ i + 1 ] ) <=
			le32_to_cpu ( mcfg->acpi.length ) ) ; i++ ) {

		/* Read allocation */
		tmp = &mcfg->alloc[i];
		DBGC2 ( &ecam, "ECAM %04x:[%02x-%02x] has base %08llx\n",
			le16_to_cpu ( tmp->segment ), tmp->start, tmp->end,
			( ( unsigned long long ) le64_to_cpu ( tmp->base ) ) );
		start = PCI_BUSDEVFN ( le16_to_cpu ( tmp->segment ),
				       tmp->start, 0, 0 );
		count = PCI_BUSDEVFN ( 0, ( tmp->end - tmp->start + 1 ), 0, 0 );

		/* Check for a matching or new closest allocation */
		index = ( busdevfn - start );
		if ( ( index < count ) || ( index > best ) ) {
			if ( alloc )
				memcpy ( alloc, tmp, sizeof ( *alloc ) );
			range->start = start;
			range->count = count;
			best = index;
		}

		/* Stop if this range contains the target bus:dev.fn address */
		if ( index < count )
			return 0;
	}

	return ( best ? 0 : -ENOENT );
}

/**
 * Find next PCI bus:dev.fn address range in system
 *
 * @v busdevfn		Starting PCI bus:dev.fn address
 * @v range		PCI bus:dev.fn address range to fill in
 */
static void ecam_discover ( uint32_t busdevfn, struct pci_range *range ) {

	/* Find new range, if any */
	ecam_find ( busdevfn, range, NULL );
}

/**
 * Access configuration space for PCI device
 *
 * @v pci		PCI device
 * @ret rc		Return status code
 */
static int ecam_access ( struct pci_device *pci ) {
	uint64_t base;
	size_t len;
	int rc;

	/* Reuse mapping if possible */
	if ( ( pci->busdevfn - ecam.range.start ) < ecam.range.count )
		return ecam.rc;

	/* Clear any existing mapping */
	if ( ecam.regs ) {
		iounmap ( ecam.regs );
		ecam.regs = NULL;
	}

	/* Find allocation for this PCI device */
	if ( ( rc = ecam_find ( pci->busdevfn, &ecam.range,
				&ecam.alloc ) ) != 0 ) {
		DBGC ( &ecam, "ECAM found no allocation for " PCI_FMT ": %s\n",
		       PCI_ARGS ( pci ), strerror ( rc ) );
		goto err_find;
	}
	if ( ecam.range.start > pci->busdevfn ) {
		DBGC ( &ecam, "ECAM found no allocation for " PCI_FMT "\n",
		       PCI_ARGS ( pci ) );
		rc = -ENOENT;
		goto err_find;
	}

	/* Map configuration space for this allocation */
	base = le64_to_cpu ( ecam.alloc.base );
	base += ( ecam.alloc.start * ECAM_SIZE * PCI_BUSDEVFN ( 0, 1, 0, 0 ) );
	len = ( ecam.range.count * ECAM_SIZE );
	if ( base != ( ( unsigned long ) base ) ) {
		DBGC ( &ecam, "ECAM %04x:[%02x-%02x] could not map "
		       "[%08llx,%08llx) outside CPU range\n",
		       le16_to_cpu ( ecam.alloc.segment ), ecam.alloc.start,
		       ecam.alloc.end, base, ( base + len ) );
		rc = -ERANGE;
		goto err_range;
	}
	ecam.regs = ioremap ( base, len );
	if ( ! ecam.regs ) {
		DBGC ( &ecam, "ECAM %04x:[%02x-%02x] could not map "
		       "[%08llx,%08llx)\n", le16_to_cpu ( ecam.alloc.segment ),
		       ecam.alloc.start, ecam.alloc.end, base, ( base + len ) );
		rc = -ENODEV;
		goto err_ioremap;
	}

	/* Populate cached mapping */
	DBGC ( &ecam, "ECAM %04x:[%02x-%02x] mapped [%08llx,%08llx) -> %p\n",
	       le16_to_cpu ( ecam.alloc.segment ), ecam.alloc.start,
	       ecam.alloc.end, base, ( base + len ), ecam.regs );
	ecam.rc = 0;
	return 0;

	iounmap ( ecam.regs );
 err_ioremap:
 err_range:
 err_find:
	ecam.rc = rc;
	return rc;
}

/**
 * Read from PCI configuration space
 *
 * @v pci	PCI device
 * @v location	Offset and length within PCI configuration space
 * @v value	Value read
 * @ret rc	Return status code
 */
int ecam_read ( struct pci_device *pci, unsigned int location, void *value ) {
	unsigned int where = ECAM_WHERE ( location );
	unsigned int len = ECAM_LEN ( location );
	unsigned int index;
	void *addr;
	int rc;

	/* Return all-ones on error */
	memset ( value, 0xff, len );

	/* Access configuration space */
	if ( ( rc = ecam_access ( pci ) ) != 0 )
		return rc;

	/* Read from address */
	index = ( pci->busdevfn - ecam.range.start );
	addr = ( ecam.regs + ( index * ECAM_SIZE ) + where );
	switch ( len ) {
	case 4:
		*( ( uint32_t *) value ) = readl ( addr );
		break;
	case 2:
		*( ( uint16_t *) value ) = readw ( addr );
		break;
	case 1:
		*( ( uint8_t *) value ) = readb ( addr );
		break;
	default:
		assert ( 0 );
	}

	return 0;
}

/**
 * Write to PCI configuration space
 *
 * @v pci	PCI device
 * @v location	Offset and length within PCI configuration space
 * @v value	Value to write
 * @ret rc	Return status code
 */
int ecam_write ( struct pci_device *pci, unsigned int location,
		 unsigned long value ) {
	unsigned int where = ECAM_WHERE ( location );
	unsigned int len = ECAM_LEN ( location );
	unsigned int index;
	void *addr;
	int rc;

	/* Access configuration space */
	if ( ( rc = ecam_access ( pci ) ) != 0 )
		return rc;

	/* Write to address */
	index = ( pci->busdevfn - ecam.range.start );
	addr = ( ecam.regs + ( index * ECAM_SIZE ) + where );
	switch ( len ) {
	case 4:
		writel ( value, addr );
		break;
	case 2:
		writew ( value, addr );
		break;
	case 1:
		writeb ( value, addr );
		break;
	default:
		assert ( 0 );
	}

	/* Read from address, to guarantee completion of the write
	 *
	 * PCIe configuration space registers may not have read side
	 * effects.  Reading back is therefore always safe to do, and
	 * guarantees that the write has reached the device.
	 */
	mb();
	ecam_read ( pci, location, &value );

	return 0;
}

PROVIDE_PCIAPI_INLINE ( ecam, pci_can_probe );
PROVIDE_PCIAPI ( ecam, pci_discover, ecam_discover );
PROVIDE_PCIAPI_INLINE ( ecam, pci_read_config_byte );
PROVIDE_PCIAPI_INLINE ( ecam, pci_read_config_word );
PROVIDE_PCIAPI_INLINE ( ecam, pci_read_config_dword );
PROVIDE_PCIAPI_INLINE ( ecam, pci_write_config_byte );
PROVIDE_PCIAPI_INLINE ( ecam, pci_write_config_word );
PROVIDE_PCIAPI_INLINE ( ecam, pci_write_config_dword );
PROVIDE_PCIAPI_INLINE ( ecam, pci_ioremap );

struct pci_api ecam_api = PCIAPI_RUNTIME ( ecam );
