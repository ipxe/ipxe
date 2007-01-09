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
#include <pnpbios.h>
#include <gpxe/pci.h>
#include <undi.h>
#include <undirom.h>
#include <undiload.h>

/** @file
 *
 * UNDI load/unload
 *
 */

/** Parameter block for calling UNDI loader */
static struct s_UNDI_LOADER __data16 ( undi_loader );
#define undi_loader __use_data16 ( undi_loader )

/** UNDI loader entry point */
static SEGOFF16_t __data16 ( undi_loader_entry );
#define undi_loader_entry __use_data16 ( undi_loader_entry )

/**
 * Call UNDI loader to create a pixie
 *
 * @v undi		UNDI device
 * @v undirom		UNDI ROM
 * @ret rc		Return status code
 */
static int undi_load ( struct undi_device *undi, struct undi_rom *undirom ) {
	struct s_PXE ppxe;
	uint16_t fbms;
	unsigned int fbms_seg;
	uint16_t exit;
	int rc;

	/* Set up START_UNDI parameters */
	memset ( &undi_loader, 0, sizeof ( undi_loader ) );
	undi_loader.AX = undi->pci_busdevfn;
	undi_loader.BX = undi->isapnp_csn;
	undi_loader.DX = undi->isapnp_read_port;
	undi_loader.ES = BIOS_SEG;
	undi_loader.DI = find_pnp_bios();

	/* Allocate base memory for PXE stack */
	get_real ( fbms, BDA_SEG, BDA_FBMS );
	undi->restore_fbms = fbms;
	fbms_seg = ( fbms << 6 );
	fbms_seg -= ( ( undirom->code_size + 0x0f ) >> 4 );
	undi_loader.UNDI_CS = fbms_seg;
	fbms_seg -= ( ( undirom->data_size + 0x0f ) >> 4 );
	undi_loader.UNDI_DS = fbms_seg;

	/* Debug info */
	DBGC ( undi, "UNDI %p loading UNDI ROM %p to CS %04x DS %04x for ",
	       undi, undirom, undi_loader.UNDI_CS, undi_loader.UNDI_DS );
	if ( undi->pci_busdevfn != 0xffff ) {
		unsigned int bus = ( undi->pci_busdevfn >> 8 );
		unsigned int devfn = ( undi->pci_busdevfn & 0xff );
		DBGC ( undi, "PCI %02x:%02x.%x\n",
		       bus, PCI_SLOT ( devfn ), PCI_FUNC ( devfn ) );
	}
	if ( undi->isapnp_csn != 0xffff ) {
		DBGC ( undi, "ISAPnP(%04x) CSN %04x\n",
		       undi->isapnp_read_port, undi->isapnp_csn );
	}

	/* Call loader */
	undi_loader_entry = undirom->loader_entry;
	__asm__ __volatile__ ( REAL_CODE ( "pushw %%ds\n\t"
					   "pushw %%ax\n\t"
					   "lcall *%c2\n\t"
					   "addw $4, %%sp\n\t" )
			       : "=a" ( exit )
			       : "a" ( & __from_data16 ( undi_loader ) ),
			         "p" ( & __from_data16 ( undi_loader_entry ) )
			       : "ebx", "ecx", "edx", "esi", "edi", "ebp" );

	/* UNDI API calls may rudely change the status of A20 and not
	 * bother to restore it afterwards.  Intel is known to be
	 * guilty of this.
	 *
	 * Note that we will return to this point even if A20 gets
	 * screwed up by the UNDI driver, because Etherboot always
	 * resides in an even megabyte of RAM.
	 */	
	gateA20_set();

	if ( exit != PXENV_EXIT_SUCCESS ) {
		rc = -undi_loader.Status;
		if ( rc == 0 ) /* Paranoia */
			rc = -EIO;
		DBGC ( undi, "UNDI %p loader failed: %s\n",
		       undi, strerror ( rc ) );
		return rc;
	}

	/* Populate PXE device structure */
	undi->pxenv = undi_loader.PXENVptr;
	undi->ppxe = undi_loader.PXEptr;
	copy_from_real ( &ppxe, undi->ppxe.segment, undi->ppxe.offset,
			 sizeof ( ppxe ) );
	undi->entry = ppxe.EntryPointSP;
	DBGC ( undi, "UNDI %p loaded PXENV+ %04x:%04x !PXE %04x:%04x "
	       "entry %04x:%04x\n", undi, undi->pxenv.segment,
	       undi->pxenv.offset, undi->ppxe.segment, undi->ppxe.offset,
	       undi->entry.segment, undi->entry.offset );

	/* Update free base memory counter */
	fbms = ( fbms_seg >> 6 );
	put_real ( fbms, BDA_SEG, BDA_FBMS );
	undi->fbms = fbms;
	DBGC ( undi, "UNDI %p using [%d,%d) kB of base memory\n",
	       undi, undi->fbms, undi->restore_fbms );

	return 0;
}

/**
 * Call UNDI loader to create a pixie
 *
 * @v undi		UNDI device
 * @v undirom		UNDI ROM
 * @v pci_busdevfn	PCI bus:dev.fn
 * @ret rc		Return status code
 */
int undi_load_pci ( struct undi_device *undi, struct undi_rom *undirom,
		    unsigned int bus, unsigned int devfn ) {
	undi->pci_busdevfn = ( ( bus << 8 ) | devfn );
	undi->isapnp_csn = 0xffff;
	undi->isapnp_read_port = 0xffff;
	return undi_load ( undi, undirom );
}

/**
 * Unload a pixie
 *
 * @v undi		UNDI device
 * @ret rc		Return status code
 *
 * Erases the PXENV+ and !PXE signatures, and frees the used base
 * memory (if possible).
 */
int undi_unload ( struct undi_device *undi ) {
	static uint32_t dead = 0xdeaddead;
	uint16_t fbms;

	DBGC ( undi, "UNDI %p unloading\n", undi );

	/* Erase signatures */
	put_real ( dead, undi->pxenv.segment, undi->pxenv.offset );
	put_real ( dead, undi->ppxe.segment, undi->ppxe.offset );

	/* Free base memory, if possible */
	get_real ( fbms, BDA_SEG, BDA_FBMS );
	if ( fbms == undi->fbms ) {
		DBGC ( undi, "UNDI %p freeing [%d,%d) kB of base memory\n",
		       undi, undi->fbms, undi->restore_fbms );
		fbms = undi->restore_fbms;
		put_real ( fbms, BDA_SEG, BDA_FBMS );
		return 0;
	} else {
		DBGC ( undi, "UNDI %p leaking [%d,%d) kB of base memory\n",
		       undi, undi->fbms, undi->restore_fbms );
		return -EBUSY;
	}
}
