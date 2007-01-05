/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <stdlib.h>
#include <string.h>
#include <pxe.h>
#include <realmode.h>
#include <bios.h>

/** @file
 *
 * PXE drivers
 *
 */

static LIST_HEAD ( pxe_drivers );

/**
 * Parse PXE ROM ID structure
 *
 * @v pxedrv		PXE driver
 * @v pxeromid		Offset within ROM to PXE ROM ID structure
 * @ret rc		Return status code
 */
static int pxedrv_parse_pxeromid ( struct pxe_driver *pxedrv,
				   unsigned int pxeromid ) {
	struct undi_rom_id undi_rom_id;
	unsigned int undiloader;

	DBGC ( pxedrv, "PXEDRV %p has PXE ROM ID at %04x:%04x\n", pxedrv,
	       pxedrv->rom_segment, pxeromid );

	/* Read PXE ROM ID structure and verify */
	copy_from_real ( &undi_rom_id, pxedrv->rom_segment, pxeromid,
			 sizeof ( undi_rom_id ) );
	if ( undi_rom_id.Signature != UNDI_ROM_ID_SIGNATURE ) {
		DBGC ( pxedrv, "PXEDRV %p has bad PXE ROM ID signature "
		       "%08lx\n", pxedrv, undi_rom_id.Signature );
		return -EINVAL;
	}

	/* Check for UNDI loader */
	undiloader = undi_rom_id.UNDILoader;
	if ( ! undiloader ) {
		DBGC ( pxedrv, "PXEDRV %p has no UNDI loader\n", pxedrv );
		return -EINVAL;
	}

	/* Fill in PXE driver loader fields */
	pxedrv->loader_entry.segment = pxedrv->rom_segment;
	pxedrv->loader_entry.offset = undiloader;
	pxedrv->code_size = undi_rom_id.CodeSize;
	pxedrv->data_size = undi_rom_id.DataSize;

	DBGC ( pxedrv, "PXEDRV %p has UNDI loader at %04x:%04x "
	       "(code %04x data %04x)\n", pxedrv,
	       pxedrv->loader_entry.segment, pxedrv->loader_entry.offset,
	       pxedrv->code_size, pxedrv->data_size );
	return 0;
}

/**
 * Parse PCI expansion header
 *
 * @v pxedrv		PXE driver
 * @v pcirheader	Offset within ROM to PCI expansion header
 */
static int pxedrv_parse_pcirheader ( struct pxe_driver *pxedrv,
				     unsigned int pcirheader ) {
	struct pcir_header pcir_header;

	DBGC ( pxedrv, "PXEDRV %p has PCI expansion header at %04x:%04x\n",
	       pxedrv, pxedrv->rom_segment, pcirheader );

	/* Read PCI expansion header and verify */
	copy_from_real ( &pcir_header, pxedrv->rom_segment, pcirheader,
			 sizeof ( pcir_header ) );
	if ( pcir_header.signature != PCIR_SIGNATURE ) {
		DBGC ( pxedrv, "PXEDRV %p has bad PCI expansion header "
		       "signature %08lx\n", pxedrv, pcir_header.signature );
		return -EINVAL;
	}
	DBGC ( pxedrv, "PXEDRV %p is a PCI ROM\n", pxedrv );

	/* Fill in PXE driver PCI device fields */
	pxedrv->bus_type = PCI_NIC;
	pxedrv->bus_id.pci.vendor_id = pcir_header.vendor_id;
	pxedrv->bus_id.pci.device_id = pcir_header.device_id;

	DBGC ( pxedrv, "PXEDRV %p is for PCI devices %04x:%04x\n", pxedrv,
	       pxedrv->bus_id.pci.vendor_id, pxedrv->bus_id.pci.device_id );
	return 0;
	
}

/**
 * Create PXE driver for expansion ROM
 *
 * @v rom_segment	ROM segment address
 * @ret rc		Return status code
 */
static int pxedrv_probe_rom ( unsigned int rom_segment ) {
	struct pxe_driver *pxedrv = NULL;
	struct undi_rom rom;
	unsigned int pxeromid;
	unsigned int pcirheader;
	int rc;

	/* Read expansion ROM header and verify */
	copy_from_real ( &rom, rom_segment, 0, sizeof ( rom ) );
	if ( rom.Signature != ROM_SIGNATURE ) {
		rc = -EINVAL;
		goto err;
	}

	/* Allocate memory for PXE driver */
	pxedrv = malloc ( sizeof ( *pxedrv ) );
	if ( ! pxedrv ) {
		DBG ( "Could not allocate PXE driver structure\n" );
		rc = -ENOMEM;
		goto err;
	}
	memset ( pxedrv, 0, sizeof ( *pxedrv ) );
	DBGC ( pxedrv, "PXEDRV %p using expansion ROM at %04x:0000 (%zdkB)\n",
	       pxedrv, rom_segment, ( rom.ROMLength / 2 ) );
	pxedrv->rom_segment = rom_segment;

	/* Check for and parse PXE ROM ID */
	pxeromid = rom.PXEROMID;
	if ( ! pxeromid ) {
		DBGC ( pxedrv, "PXEDRV %p has no PXE ROM ID\n", pxedrv );
		rc = -EINVAL;
		goto err;
	}
	if ( ( rc = pxedrv_parse_pxeromid ( pxedrv, pxeromid ) ) != 0 )
		goto err;

	/* Parse PCIR header, if present */
	pcirheader = rom.PCIRHeader;
	if ( pcirheader )
		pxedrv_parse_pcirheader ( pxedrv, pcirheader );

	/* Add to PXE driver list and return */
	DBGC ( pxedrv, "PXEDRV %p registered\n", pxedrv );
	list_add ( &pxedrv->list, &pxe_drivers );
	return 0;

 err:
	free ( pxedrv );
	return rc;
}

/**
 * Create PXE drivers for all possible expansion ROMs
 *
 * @ret 
 */
static void pxedrv_probe_all_roms ( void ) {
	static int probed = 0;
	unsigned int rom_segment;

	/* Perform probe only once */
	if ( probed )
		return;

	DBG ( "Scanning for PXE expansion ROMs\n" );

	/* Scan through expansion ROM region at 2kB intervals */
	for ( rom_segment = 0xc000 ; rom_segment < 0x10000 ;
	      rom_segment += 0x80 ) {
		pxedrv_probe_rom ( rom_segment );
	}

	probed = 1;
}

/**
 * Find PXE driver for PCI device
 *
 * @v vendor_id		PCI vendor ID
 * @v device_id		PCI device ID
 * @v rombase		ROM base address, or 0 for any
 * @ret pxedrv		PXE driver, or NULL
 */
struct pxe_driver * pxedrv_find_pci_driver ( unsigned int vendor_id,
					     unsigned int device_id,
					     unsigned int rombase ) {
	struct pxe_driver *pxedrv;

	pxedrv_probe_all_roms();

	list_for_each_entry ( pxedrv, &pxe_drivers, list ) {
		if ( pxedrv->bus_type != PCI_NIC )
			continue;
		if ( pxedrv->bus_id.pci.vendor_id != vendor_id )
			continue;
		if ( pxedrv->bus_id.pci.device_id != device_id )
			continue;
		if ( rombase && ( ( pxedrv->rom_segment << 4 ) != rombase ) )
			continue;
		DBGC ( pxedrv, "PXEDRV %p matched PCI %04x:%04x (%08x)\n",
		       pxedrv, vendor_id, device_id, rombase );
		return pxedrv;
	}

	DBG ( "No PXE driver matched PCI %04x:%04x (%08x)\n",
	      vendor_id, device_id, rombase );
	return NULL;
}

/** Parameter block for calling UNDI loader */
static struct s_UNDI_LOADER __data16 ( undi_loader );
#define undi_loader __use_data16 ( undi_loader )

/** UNDI loader entry point */
static SEGOFF16_t __data16 ( undi_loader_entry );
#define undi_loader_entry __use_data16 ( undi_loader_entry )

/**
 * Call UNDI loader to create a pixie
 *
 * @v pxedrv		PXE driver
 * @v pxe		PXE device to be created
 * @v pci_busdevfn	PCI bus:dev.fn (PCI devices only), or 0
 * @v isapnp_csn	ISAPnP Card Select Number, or -1U
 * @v isapnp_read_port	ISAPnP read port, or -1U
 * @ret rc		Return status code
 */
static int pxedrv_load ( struct pxe_driver *pxedrv, struct pxe_device *pxe,
			 unsigned int pci_busdevfn, unsigned int isapnp_csn,
			 unsigned int isapnp_read_port ) {
	int discard;
	uint16_t exit;
	uint16_t fbms;
	unsigned int fbms_seg;
	int rc;

	memset ( &undi_loader, 0, sizeof ( undi_loader ) );
	undi_loader.AX = pci_busdevfn;
	undi_loader.BX = isapnp_csn;
	undi_loader.DX = isapnp_read_port;

	/* Allocate base memory for PXE stack */
	get_real ( fbms, BDA_SEG, BDA_FBMS );
	fbms_seg = ( fbms << 6 );
	fbms_seg -= ( ( pxedrv->data_size + 0x0f ) >> 4 );
	undi_loader.UNDI_DS = fbms_seg;
	fbms_seg -= ( ( pxedrv->code_size + 0x0f ) >> 4 );
	undi_loader.UNDI_CS = fbms_seg;
	DBGC ( pxedrv, "PXEDRV %p loading to CS %04x and DS %04x\n", pxedrv,
	       undi_loader.UNDI_CS, undi_loader.UNDI_DS );

	/* Call loader */
	undi_loader_entry = pxedrv->loader_entry;
	__asm__ __volatile__ ( REAL_CODE ( "pushw %%ds\n\t"
					   "pushw %w0\n\t"
					   "lcall *%c3\n\t"
					   "addw $4, %%sp\n\t" )
			       : "=a" ( exit ), "=r" ( discard )
			       : "0" ( & __from_data16 ( undi_loader ) ),
			         "p" ( & __from_data16 ( undi_loader_entry )));
	if ( exit != PXENV_EXIT_SUCCESS ) {
		rc = -undi_loader.Status;
		if ( rc == 0 ) /* Paranoia */
			rc = -EIO;
		DBGC ( pxedrv, "PXEDRV %p loader failed: %s\n",
		       strerror ( rc ) );
		return rc;
	}

	/* Update free base memory counter */
	fbms = ( fbms_seg >> 6 );
	put_real ( fbms, BDA_SEG, BDA_FBMS );

	/* Record location of pixie in PXE device structure */
	pxe->pxenv = undi_loader.PXENVptr;
	pxe->ppxe = undi_loader.PXEptr;
	return 0;
}

/**
 * Call UNDI loader to create a pixie
 *
 * @v pxedrv		PXE driver
 * @v pxe		PXE device to be created
 * @v pci_busdevfn	PCI bus:dev.fn
 * @ret rc		Return status code
 */
int pxedrv_load_pci ( struct pxe_driver *pxedrv, struct pxe_device *pxe,
		      unsigned int bus, unsigned int devfn ) {
	return pxedrv_load ( pxedrv, pxe, ( ( bus << 8 ) | devfn ), -1U, -1U );
}
