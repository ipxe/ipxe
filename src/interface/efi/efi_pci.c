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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ipxe/pci.h>
#include <ipxe/acpi.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_pci.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/Protocol/PciIo.h>
#include <ipxe/efi/Protocol/PciRootBridgeIo.h>

/** @file
 *
 * iPXE PCI I/O API for EFI
 *
 */

/* Disambiguate the various error causes */
#define EINFO_EEFI_PCI							\
	__einfo_uniqify ( EINFO_EPLATFORM, 0x01,			\
			  "Could not open PCI I/O protocol" )
#define EINFO_EEFI_PCI_NOT_PCI						\
	__einfo_platformify ( EINFO_EEFI_PCI, EFI_UNSUPPORTED,		\
			      "Not a PCI device" )
#define EEFI_PCI_NOT_PCI __einfo_error ( EINFO_EEFI_PCI_NOT_PCI )
#define EINFO_EEFI_PCI_IN_USE						\
	__einfo_platformify ( EINFO_EEFI_PCI, EFI_ACCESS_DENIED,	\
			      "PCI device already has a driver" )
#define EEFI_PCI_IN_USE __einfo_error ( EINFO_EEFI_PCI_IN_USE )
#define EEFI_PCI( efirc )						\
	EPLATFORM ( EINFO_EEFI_PCI, efirc,				\
		    EEFI_PCI_NOT_PCI, EEFI_PCI_IN_USE )

/******************************************************************************
 *
 * iPXE PCI API
 *
 ******************************************************************************
 */

/**
 * Find closest bus:dev.fn address range within a root bridge
 *
 * @v pci		Starting PCI device
 * @v handle		EFI PCI root bridge handle
 * @v range		PCI bus:dev.fn address range to fill in
 * @ret rc		Return status code
 */
static int efipci_discover_one ( struct pci_device *pci, EFI_HANDLE handle,
				 struct pci_range *range ) {
	EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *root;
	union {
		union acpi_resource *res;
		void *raw;
	} acpi;
	uint32_t best = 0;
	uint32_t start;
	uint32_t count;
	uint32_t index;
	unsigned int tag;
	EFI_STATUS efirc;
	int rc;

	/* Return empty range on error */
	range->start = 0;
	range->count = 0;

	/* Open root bridge I/O protocol */
	if ( ( rc = efi_open ( handle, &efi_pci_root_bridge_io_protocol_guid,
			       &root ) ) != 0 ) {
		DBGC ( pci, "EFIPCI " PCI_FMT " cannot open %s: %s\n",
		       PCI_ARGS ( pci ), efi_handle_name ( handle ),
		       strerror ( rc ) );
		return rc;
	}

	/* Get ACPI resource descriptors */
	if ( ( efirc = root->Configuration ( root, &acpi.raw ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( pci, "EFIPCI " PCI_FMT " cannot get configuration for "
		       "%s: %s\n", PCI_ARGS ( pci ),
		       efi_handle_name ( handle ), strerror ( rc ) );
		return rc;
	}

	/* Parse resource descriptors */
	for ( ; ( ( tag = acpi_resource_tag ( acpi.res ) ) !=
		  ACPI_END_RESOURCE ) ;
	      acpi.res = acpi_resource_next ( acpi.res ) ) {

		/* Ignore anything other than a bus number range descriptor */
		if ( tag != ACPI_QWORD_ADDRESS_SPACE_RESOURCE )
			continue;
		if ( acpi.res->qword.type != ACPI_ADDRESS_TYPE_BUS )
			continue;

		/* Get range for this descriptor */
		start = PCI_BUSDEVFN ( root->SegmentNumber,
				       le64_to_cpu ( acpi.res->qword.min ),
				       0, 0 );
		count = PCI_BUSDEVFN ( 0, le64_to_cpu ( acpi.res->qword.len ),
				       0, 0 );
		DBGC2 ( pci, "EFIPCI " PCI_FMT " found %04x:[%02x-%02x] via "
			"%s\n", PCI_ARGS ( pci ), root->SegmentNumber,
			PCI_BUS ( start ), PCI_BUS ( start + count - 1 ),
			efi_handle_name ( handle ) );

		/* Check for a matching or new closest range */
		index = ( pci->busdevfn - start );
		if ( ( index < count ) || ( index > best ) ) {
			range->start = start;
			range->count = count;
			best = index;
		}

		/* Stop if this range contains the target bus:dev.fn address */
		if ( index < count )
			break;
	}

	/* If no range descriptors were seen, assume that the root
	 * bridge has a single bus.
	 */
	if ( ! range->count ) {
		range->start = PCI_BUSDEVFN ( root->SegmentNumber, 0, 0, 0 );
		range->count = PCI_BUSDEVFN ( 0, 1, 0, 0 );
	}

	return 0;
}

/**
 * Find closest bus:dev.fn address range within any root bridge
 *
 * @v pci		Starting PCI device
 * @v range		PCI bus:dev.fn address range to fill in
 * @v handle		PCI root bridge I/O handle to fill in
 * @ret rc		Return status code
 */
static int efipci_discover_any ( struct pci_device *pci,
				 struct pci_range *range,
				 EFI_HANDLE *handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	uint32_t best = 0;
	uint32_t index;
	struct pci_range tmp;
	EFI_HANDLE *handles;
	UINTN num_handles;
	UINTN i;
	EFI_STATUS efirc;
	int rc;

	/* Return an empty range and no handle on error */
	range->start = 0;
	range->count = 0;
	*handle = NULL;

	/* Enumerate all root bridge I/O protocol handles */
	if ( ( efirc = bs->LocateHandleBuffer ( ByProtocol,
			&efi_pci_root_bridge_io_protocol_guid,
			NULL, &num_handles, &handles ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( pci, "EFIPCI " PCI_FMT " cannot locate root bridges: "
		       "%s\n", PCI_ARGS ( pci ), strerror ( rc ) );
		goto err_locate;
	}

	/* Iterate over all root bridge I/O protocols */
	for ( i = 0 ; i < num_handles ; i++ ) {

		/* Get matching or closest range for this root bridge */
		if ( ( rc = efipci_discover_one ( pci, handles[i],
						  &tmp ) ) != 0 )
			continue;

		/* Check for a matching or new closest range */
		index = ( pci->busdevfn - tmp.start );
		if ( ( index < tmp.count ) || ( index > best ) ) {
			range->start = tmp.start;
			range->count = tmp.count;
			best = index;
		}

		/* Stop if this range contains the target bus:dev.fn address */
		if ( index < tmp.count ) {
			*handle = handles[i];
			break;
		}
	}

	/* Check for a range containing the target bus:dev.fn address */
	if ( ! *handle ) {
		rc = -ENOENT;
		goto err_range;
	}

	/* Success */
	rc = 0;

 err_range:
	bs->FreePool ( handles );
 err_locate:
	return rc;
}

/**
 * Find next PCI bus:dev.fn address range in system
 *
 * @v busdevfn		Starting PCI bus:dev.fn address
 * @v range		PCI bus:dev.fn address range to fill in
 */
static void efipci_discover ( uint32_t busdevfn, struct pci_range *range ) {
	struct pci_device pci;
	EFI_HANDLE handle;

	/* Find range */
	memset ( &pci, 0, sizeof ( pci ) );
	pci_init ( &pci, busdevfn );
	efipci_discover_any ( &pci, range, &handle );
}

/**
 * Open EFI PCI root bridge I/O protocol for ephemeral use
 *
 * @v pci		PCI device
 * @ret handle		EFI PCI root bridge handle
 * @ret root		EFI PCI root bridge I/O protocol, or NULL if not found
 * @ret rc		Return status code
 */
static int efipci_root_open ( struct pci_device *pci, EFI_HANDLE *handle,
			      EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL **root ) {
	struct pci_range tmp;
	int rc;

	/* Find matching root bridge I/O protocol handle */
	if ( ( rc = efipci_discover_any ( pci, &tmp, handle ) ) != 0 )
		return rc;

	/* Open PCI root bridge I/O protocol */
	if ( ( rc = efi_open ( *handle, &efi_pci_root_bridge_io_protocol_guid,
			       root ) ) != 0 ) {
		DBGC ( pci, "EFIPCI " PCI_FMT " cannot open %s: %s\n",
		       PCI_ARGS ( pci ), efi_handle_name ( *handle ),
		       strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Calculate EFI PCI configuration space address
 *
 * @v pci		PCI device
 * @v location		Encoded offset and width
 * @ret address		EFI PCI address
 */
static unsigned long efipci_address ( struct pci_device *pci,
				      unsigned long location ) {

	return EFI_PCI_ADDRESS ( PCI_BUS ( pci->busdevfn ),
				 PCI_SLOT ( pci->busdevfn ),
				 PCI_FUNC ( pci->busdevfn ),
				 EFIPCI_OFFSET ( location ) );
}

/**
 * Read from PCI configuration space
 *
 * @v pci		PCI device
 * @v location		Encoded offset and width
 * @ret value		Value
 * @ret rc		Return status code
 */
int efipci_read ( struct pci_device *pci, unsigned long location,
		  void *value ) {
	EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *root;
	EFI_HANDLE handle;
	EFI_STATUS efirc;
	int rc;

	/* Open root bridge */
	if ( ( rc = efipci_root_open ( pci, &handle, &root ) ) != 0 )
		return rc;

	/* Read from configuration space */
	if ( ( efirc = root->Pci.Read ( root, EFIPCI_WIDTH ( location ),
					efipci_address ( pci, location ), 1,
					value ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( pci, "EFIPCI " PCI_FMT " config read from offset %02lx "
		       "failed: %s\n", PCI_ARGS ( pci ),
		       EFIPCI_OFFSET ( location ), strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Write to PCI configuration space
 *
 * @v pci		PCI device
 * @v location		Encoded offset and width
 * @v value		Value
 * @ret rc		Return status code
 */
int efipci_write ( struct pci_device *pci, unsigned long location,
		   unsigned long value ) {
	EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *root;
	EFI_HANDLE handle;
	EFI_STATUS efirc;
	int rc;

	/* Open root bridge */
	if ( ( rc = efipci_root_open ( pci, &handle, &root ) ) != 0 )
		return rc;

	/* Read from configuration space */
	if ( ( efirc = root->Pci.Write ( root, EFIPCI_WIDTH ( location ),
					 efipci_address ( pci, location ), 1,
					 &value ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( pci, "EFIPCI " PCI_FMT " config write to offset %02lx "
		       "failed: %s\n", PCI_ARGS ( pci ),
		       EFIPCI_OFFSET ( location ), strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Map PCI bus address as an I/O address
 *
 * @v bus_addr		PCI bus address
 * @v len		Length of region
 * @ret io_addr		I/O address, or NULL on error
 */
void * efipci_ioremap ( struct pci_device *pci, unsigned long bus_addr,
			size_t len ) {
	union {
		union acpi_resource *res;
		void *raw;
	} u;
	EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *root;
	EFI_HANDLE handle;
	unsigned int tag;
	uint64_t offset;
	uint64_t start;
	uint64_t end;
	EFI_STATUS efirc;
	int rc;

	/* Open root bridge */
	if ( ( rc = efipci_root_open ( pci, &handle, &root ) ) != 0 )
		goto err_root;

	/* Get ACPI resource descriptors */
	if ( ( efirc = root->Configuration ( root, &u.raw ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( pci, "EFIPCI " PCI_FMT " cannot get configuration: "
		       "%s\n", PCI_ARGS ( pci ), strerror ( rc ) );
		goto err_config;
	}

	/* Parse resource descriptors */
	for ( ; ( ( tag = acpi_resource_tag ( u.res ) ) != ACPI_END_RESOURCE ) ;
	      u.res = acpi_resource_next ( u.res ) ) {

		/* Ignore anything other than a memory range descriptor */
		if ( tag != ACPI_QWORD_ADDRESS_SPACE_RESOURCE )
			continue;
		if ( u.res->qword.type != ACPI_ADDRESS_TYPE_MEM )
			continue;

		/* Ignore descriptors that do not cover this memory range */
		offset = le64_to_cpu ( u.res->qword.offset );
		start = ( offset + le64_to_cpu ( u.res->qword.min ) );
		end = ( start + le64_to_cpu ( u.res->qword.len ) );
		DBGC2 ( pci, "EFIPCI " PCI_FMT " found range [%08llx,%08llx) "
			"-> [%08llx,%08llx)\n", PCI_ARGS ( pci ), start, end,
			( start - offset ), ( end - offset ) );
		if ( ( bus_addr < start ) || ( ( bus_addr + len ) > end ) )
			continue;

		/* Use this address space descriptor */
		DBGC2 ( pci, "EFIPCI " PCI_FMT " %08lx+%zx -> ",
			PCI_ARGS ( pci ), bus_addr, len );
		bus_addr -= offset;
		DBGC2 ( pci, "%08lx\n", bus_addr );
		break;
	}
	if ( tag == ACPI_END_RESOURCE ) {
		DBGC ( pci, "EFIPCI " PCI_FMT " %08lx+%zx is not within "
		       "root bridge address space\n",
		       PCI_ARGS ( pci ), bus_addr, len );
	}

 err_config:
 err_root:
	return ioremap ( bus_addr, len );
}

PROVIDE_PCIAPI_INLINE ( efi, pci_can_probe );
PROVIDE_PCIAPI ( efi, pci_discover, efipci_discover );
PROVIDE_PCIAPI_INLINE ( efi, pci_read_config_byte );
PROVIDE_PCIAPI_INLINE ( efi, pci_read_config_word );
PROVIDE_PCIAPI_INLINE ( efi, pci_read_config_dword );
PROVIDE_PCIAPI_INLINE ( efi, pci_write_config_byte );
PROVIDE_PCIAPI_INLINE ( efi, pci_write_config_word );
PROVIDE_PCIAPI_INLINE ( efi, pci_write_config_dword );
PROVIDE_PCIAPI ( efi, pci_ioremap, efipci_ioremap );

/******************************************************************************
 *
 * EFI PCI DMA mappings
 *
 ******************************************************************************
 */

/**
 * Map buffer for DMA
 *
 * @v dma		DMA device
 * @v map		DMA mapping to fill in
 * @v addr		Buffer address
 * @v len		Length of buffer
 * @v flags		Mapping flags
 * @ret rc		Return status code
 */
static int efipci_dma_map ( struct dma_device *dma, struct dma_mapping *map,
			    physaddr_t addr, size_t len, int flags ) {
	struct efi_pci_device *efipci =
		container_of ( dma, struct efi_pci_device, pci.dma );
	struct pci_device *pci = &efipci->pci;
	EFI_PCI_IO_PROTOCOL *pci_io = efipci->io;
	EFI_PCI_IO_PROTOCOL_OPERATION op;
	EFI_PHYSICAL_ADDRESS bus;
	UINTN count;
	VOID *mapping;
	EFI_STATUS efirc;
	int rc;

	/* Sanity check */
	assert ( map->dma == NULL );
	assert ( map->offset == 0 );
	assert ( map->token == NULL );

	/* Determine operation */
	switch ( flags ) {
	case DMA_TX:
		op = EfiPciIoOperationBusMasterRead;
		break;
	case DMA_RX:
		op = EfiPciIoOperationBusMasterWrite;
		break;
	default:
		op = EfiPciIoOperationBusMasterCommonBuffer;
		break;
	}

	/* Map buffer (if non-zero length) */
	count = len;
	if ( len ) {
		if ( ( efirc = pci_io->Map ( pci_io, op, phys_to_virt ( addr ),
					     &count, &bus, &mapping ) ) != 0 ) {
			rc = -EEFI ( efirc );
			DBGC ( pci, "EFIPCI " PCI_FMT " cannot map %08lx+%zx: "
			       "%s\n", PCI_ARGS ( pci ), addr, len,
			       strerror ( rc ) );
			goto err_map;
		}
	} else {
		bus = addr;
		mapping = NULL;
	}

	/* Check that full length was mapped.  The UEFI specification
	 * allows for multiple mappings to be required, but even the
	 * EDK2 PCI device drivers will fail if a platform ever
	 * requires this.
	 */
	if ( count != len ) {
		DBGC ( pci, "EFIPCI " PCI_FMT " attempted split mapping for "
		       "%08lx+%zx\n", PCI_ARGS ( pci ), addr, len );
		rc = -ENOTSUP;
		goto err_len;
	}

	/* Populate mapping */
	map->dma = dma;
	map->offset = ( bus - addr );
	map->token = mapping;

	/* Increment mapping count (for debugging) */
	if ( DBG_LOG )
		dma->mapped++;

	return 0;

 err_len:
	pci_io->Unmap ( pci_io, mapping );
 err_map:
	return rc;
}

/**
 * Unmap buffer
 *
 * @v dma		DMA device
 * @v map		DMA mapping
 */
static void efipci_dma_unmap ( struct dma_device *dma,
			       struct dma_mapping *map ) {
	struct efi_pci_device *efipci =
		container_of ( dma, struct efi_pci_device, pci.dma );
	EFI_PCI_IO_PROTOCOL *pci_io = efipci->io;

	/* Unmap buffer (if non-zero length) */
	if ( map->token )
		pci_io->Unmap ( pci_io, map->token );

	/* Clear mapping */
	map->dma = NULL;
	map->offset = 0;
	map->token = NULL;

	/* Decrement mapping count (for debugging) */
	if ( DBG_LOG )
		dma->mapped--;
}

/**
 * Allocate and map DMA-coherent buffer
 *
 * @v dma		DMA device
 * @v map		DMA mapping to fill in
 * @v len		Length of buffer
 * @v align		Physical alignment
 * @ret addr		Buffer address, or NULL on error
 */
static void * efipci_dma_alloc ( struct dma_device *dma,
				 struct dma_mapping *map,
				 size_t len, size_t align __unused ) {
	struct efi_pci_device *efipci =
		container_of ( dma, struct efi_pci_device, pci.dma );
	struct pci_device *pci = &efipci->pci;
	EFI_PCI_IO_PROTOCOL *pci_io = efipci->io;
	unsigned int pages;
	VOID *addr;
	EFI_STATUS efirc;
	int rc;

	/* Calculate number of pages */
	pages = ( ( len + EFI_PAGE_SIZE - 1 ) / EFI_PAGE_SIZE );

	/* Allocate (page-aligned) buffer */
	if ( ( efirc = pci_io->AllocateBuffer ( pci_io, AllocateAnyPages,
						EfiBootServicesData, pages,
						&addr, 0 ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( pci, "EFIPCI " PCI_FMT " could not allocate %zd bytes: "
		       "%s\n", PCI_ARGS ( pci ), len, strerror ( rc ) );
		goto err_alloc;
	}

	/* Clear buffer */
	memset ( addr, 0, ( pages * EFI_PAGE_SIZE ) );

	/* Map buffer */
	if ( ( rc = efipci_dma_map ( dma, map, virt_to_phys ( addr ),
				     ( pages * EFI_PAGE_SIZE ),
				     DMA_BI ) ) != 0 )
		goto err_map;

	/* Increment allocation count (for debugging) */
	if ( DBG_LOG )
		dma->allocated++;

	return addr;

	efipci_dma_unmap ( dma, map );
 err_map:
	pci_io->FreeBuffer ( pci_io, pages, addr );
 err_alloc:
	return NULL;
}

/**
 * Unmap and free DMA-coherent buffer
 *
 * @v dma		DMA device
 * @v map		DMA mapping
 * @v addr		Buffer address
 * @v len		Length of buffer
 */
static void efipci_dma_free ( struct dma_device *dma, struct dma_mapping *map,
			      void *addr, size_t len ) {
	struct efi_pci_device *efipci =
		container_of ( dma, struct efi_pci_device, pci.dma );
	EFI_PCI_IO_PROTOCOL *pci_io = efipci->io;
	unsigned int pages;

	/* Calculate number of pages */
	pages = ( ( len + EFI_PAGE_SIZE - 1 ) / EFI_PAGE_SIZE );

	/* Unmap buffer */
	efipci_dma_unmap ( dma, map );

	/* Free buffer */
	pci_io->FreeBuffer ( pci_io, pages, addr );

	/* Decrement allocation count (for debugging) */
	if ( DBG_LOG )
		dma->allocated--;
}

/**
 * Set addressable space mask
 *
 * @v dma		DMA device
 * @v mask		Addressable space mask
 */
static void efipci_dma_set_mask ( struct dma_device *dma, physaddr_t mask ) {
	struct efi_pci_device *efipci =
		container_of ( dma, struct efi_pci_device, pci.dma );
	struct pci_device *pci = &efipci->pci;
	EFI_PCI_IO_PROTOCOL *pci_io = efipci->io;
	EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op;
	UINT64 attrs;
	int is64;
	EFI_STATUS efirc;
	int rc;

	/* Set dual address cycle attribute for 64-bit capable devices */
	is64 = ( ( ( ( uint64_t ) mask ) + 1 ) == 0 );
	op = ( is64 ? EfiPciIoAttributeOperationEnable :
	       EfiPciIoAttributeOperationDisable );
	attrs = EFI_PCI_IO_ATTRIBUTE_DUAL_ADDRESS_CYCLE;
	if ( ( efirc = pci_io->Attributes ( pci_io, op, attrs, NULL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( pci, "EFIPCI " PCI_FMT " could not %sable DAC: %s\n",
		       PCI_ARGS ( pci ), ( is64 ? "en" : "dis" ),
		       strerror ( rc ) );
		/* Ignore failure: errors will manifest in mapping attempts */
		return;
	}
}

/** EFI PCI DMA operations */
static struct dma_operations efipci_dma_operations = {
	.map = efipci_dma_map,
	.unmap = efipci_dma_unmap,
	.alloc = efipci_dma_alloc,
	.free = efipci_dma_free,
	.umalloc = efipci_dma_alloc,
	.ufree = efipci_dma_free,
	.set_mask = efipci_dma_set_mask,
};

/******************************************************************************
 *
 * EFI PCI device instantiation
 *
 ******************************************************************************
 */

/**
 * Get EFI PCI device information
 *
 * @v device		EFI device handle
 * @v efipci		EFI PCI device to fill in
 * @ret rc		Return status code
 */
int efipci_info ( EFI_HANDLE device, struct efi_pci_device *efipci ) {
	EFI_PCI_IO_PROTOCOL *pci_io;
	UINTN pci_segment, pci_bus, pci_dev, pci_fn;
	unsigned int busdevfn;
	EFI_STATUS efirc;
	int rc;

	/* See if device is a PCI device */
	if ( ( rc = efi_open ( device, &efi_pci_io_protocol_guid,
			       &pci_io ) ) != 0 ) {
		DBGCP ( device, "EFIPCI %s cannot open PCI protocols: %s\n",
			efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}
	efipci->io = pci_io;

	/* Get PCI bus:dev.fn address */
	if ( ( efirc = pci_io->GetLocation ( pci_io, &pci_segment, &pci_bus,
					     &pci_dev, &pci_fn ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFIPCI %s could not get PCI location: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}
	busdevfn = PCI_BUSDEVFN ( pci_segment, pci_bus, pci_dev, pci_fn );
	pci_init ( &efipci->pci, busdevfn );
	dma_init ( &efipci->pci.dma, &efipci_dma_operations );
	DBGCP ( device, "EFIPCI " PCI_FMT " is %s\n",
		PCI_ARGS ( &efipci->pci ), efi_handle_name ( device ) );

	/* Try to enable I/O cycles, memory cycles, and bus mastering.
	 * Some platforms will 'helpfully' report errors if these bits
	 * can't be enabled (for example, if the card doesn't actually
	 * support I/O cycles).  Work around any such platforms by
	 * enabling bits individually and simply ignoring any errors.
	 */
	pci_io->Attributes ( pci_io, EfiPciIoAttributeOperationEnable,
			     EFI_PCI_IO_ATTRIBUTE_IO, NULL );
	pci_io->Attributes ( pci_io, EfiPciIoAttributeOperationEnable,
			     EFI_PCI_IO_ATTRIBUTE_MEMORY, NULL );
	pci_io->Attributes ( pci_io, EfiPciIoAttributeOperationEnable,
			     EFI_PCI_IO_ATTRIBUTE_BUS_MASTER, NULL );

	/* Populate PCI device */
	if ( ( rc = pci_read_config ( &efipci->pci ) ) != 0 ) {
		DBGC ( device, "EFIPCI " PCI_FMT " cannot read PCI "
		       "configuration: %s\n",
		       PCI_ARGS ( &efipci->pci ), strerror ( rc ) );
		return rc;
	}

	return 0;
}

/******************************************************************************
 *
 * EFI PCI driver
 *
 ******************************************************************************
 */

/**
 * Check to see if driver supports a device
 *
 * @v device		EFI device handle
 * @ret rc		Return status code
 */
static int efipci_supported ( EFI_HANDLE device ) {
	struct efi_pci_device efipci;
	uint8_t hdrtype;
	int rc;

	/* Get PCI device information */
	if ( ( rc = efipci_info ( device, &efipci ) ) != 0 )
		return rc;

	/* Do not attempt to drive bridges */
	hdrtype = efipci.pci.hdrtype;
	if ( ( hdrtype & PCI_HEADER_TYPE_MASK ) != PCI_HEADER_TYPE_NORMAL ) {
		DBGC ( device, "EFIPCI " PCI_FMT " type %02x is not type %02x\n",
		       PCI_ARGS ( &efipci.pci ), hdrtype,
		       PCI_HEADER_TYPE_NORMAL );
		return -ENOTTY;
	}

	/* Look for a driver */
	if ( ( rc = pci_find_driver ( &efipci.pci ) ) != 0 ) {
		DBGC ( device, "EFIPCI " PCI_FMT " (%04x:%04x class %06x) "
		       "has no driver\n", PCI_ARGS ( &efipci.pci ),
		       efipci.pci.vendor, efipci.pci.device,
		       efipci.pci.class );
		return rc;
	}
	DBGC ( device, "EFIPCI " PCI_FMT " (%04x:%04x class %06x) has driver "
	       "\"%s\"\n", PCI_ARGS ( &efipci.pci ), efipci.pci.vendor,
	       efipci.pci.device, efipci.pci.class, efipci.pci.id->name );

	return 0;
}

/**
 * Exclude existing drivers
 *
 * @v device		EFI device handle
 * @ret rc		Return status code
 */
static int efipci_exclude ( EFI_HANDLE device ) {
	EFI_GUID *protocol = &efi_pci_io_protocol_guid;
	int rc;

	/* Exclude existing PCI I/O protocol drivers */
	if ( ( rc = efi_driver_exclude ( device, protocol ) ) != 0 ) {
		DBGC ( device, "EFIPCI %s could not exclude drivers: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Attach driver to device
 *
 * @v efidev		EFI device
 * @ret rc		Return status code
 */
static int efipci_start ( struct efi_device *efidev ) {
	EFI_HANDLE device = efidev->device;
	struct efi_pci_device *efipci;
	int rc;

	/* Allocate PCI device */
	efipci = zalloc ( sizeof ( *efipci ) );
	if ( ! efipci ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Get PCI device information */
	if ( ( rc = efipci_info ( device, efipci ) ) != 0 )
		goto err_info;

	/* Open PCI I/O protocol */
	if ( ( rc = efi_open_by_driver ( device, &efi_pci_io_protocol_guid,
					 &efipci->io ) ) != 0 ) {
		DBGC ( device, "EFIPCI %s could not open PCI device: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		DBGC_EFI_OPENERS ( device, device, &efi_pci_io_protocol_guid );
		goto err_open;
	}

	/* Find driver */
	if ( ( rc = pci_find_driver ( &efipci->pci ) ) != 0 ) {
		DBGC ( device, "EFIPCI " PCI_FMT " has no driver\n",
		       PCI_ARGS ( &efipci->pci ) );
		goto err_find_driver;
	}

	/* Mark PCI device as a child of the EFI device */
	efipci->pci.dev.parent = &efidev->dev;
	list_add ( &efipci->pci.dev.siblings, &efidev->dev.children );

	/* Probe driver */
	if ( ( rc = pci_probe ( &efipci->pci ) ) != 0 ) {
		DBGC ( device, "EFIPCI " PCI_FMT " could not probe driver "
		       "\"%s\": %s\n", PCI_ARGS ( &efipci->pci ),
		       efipci->pci.id->name, strerror ( rc ) );
		goto err_probe;
	}
	DBGC ( device, "EFIPCI " PCI_FMT " using driver \"%s\"\n",
	       PCI_ARGS ( &efipci->pci ), efipci->pci.id->name );

	efidev_set_drvdata ( efidev, efipci );
	return 0;

	pci_remove ( &efipci->pci );
 err_probe:
	list_del ( &efipci->pci.dev.siblings );
 err_find_driver:
	efi_close_by_driver ( device, &efi_pci_io_protocol_guid );
 err_open:
 err_info:
	free ( efipci );
 err_alloc:
	return rc;
}

/**
 * Detach driver from device
 *
 * @v efidev		EFI device
  */
static void efipci_stop ( struct efi_device *efidev ) {
	struct efi_pci_device *efipci = efidev_get_drvdata ( efidev );
	EFI_HANDLE device = efidev->device;

	pci_remove ( &efipci->pci );
	list_del ( &efipci->pci.dev.siblings );
	assert ( efipci->pci.dma.mapped == 0 );
	assert ( efipci->pci.dma.allocated == 0 );
	efi_close_by_driver ( device, &efi_pci_io_protocol_guid );
	free ( efipci );
}

/** EFI PCI driver */
struct efi_driver efipci_driver __efi_driver ( EFI_DRIVER_HARDWARE ) = {
	.name = "PCI",
	.supported = efipci_supported,
	.exclude = efipci_exclude,
	.start = efipci_start,
	.stop = efipci_stop,
};
