#if 0

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
#include <assert.h>
#include <pxe.h>
#include <realmode.h>
#include <bios.h>
#include <pnpbios.h>
#include <gpxe/device.h>
#include <gpxe/pci.h>

/** @file
 *
 * PXE bus
 *
 */

/**
 * Byte checksum
 *
 * @v data		Data to checksum
 * @v len		Length of data
 * @ret sum		Byte checksum
 */
static uint8_t checksum ( void *data, size_t len ) {
	uint8_t *bytes = data;
	unsigned int sum = 0;

	while ( len-- )
		sum += *(bytes++);

	return sum;
}

/**
 * Unregister a PXE device
 *
 * @v pxe		PXE device
 *
 */
static void unregister_pxedev ( struct pxe_device *pxe ) {
	undi_remove ( pxe );
	list_del ( &pxe->dev.siblings );
	DBGC ( pxe, "PXE %p unregistered\n", pxe );
}

static void pxebus_remove ( struct root_device *rootdev );

/**
 * Probe PXE root bus
 *
 * @v rootdev		PXE bus root device
 *
 * Scans the PXE bus for devices and registers all devices it can
 * find.
 */
static int pxebus_probe ( struct root_device *rootdev ) {
	struct pxe_device *pxe = NULL;
	struct s_PXENV pxenv;
	struct s_PXE ppxe;
	uint16_t fbms;
	unsigned int segment;
	unsigned int undi_cs;
	int rc;

	/* Scan through allocated base memory for PXENV+ structure */
	get_real ( fbms, BDA_SEG, BDA_FBMS );
	for ( segment = ( fbms << 6 ) ; segment < 0xa000 ; segment++ ) {

		/* Verify PXENV+ signature and checksum */
		copy_from_real ( &pxenv, segment, 0, sizeof ( pxenv ) );
		if ( memcmp ( pxenv.Signature, "PXENV+", 6 ) != 0 )
			continue;
		DBG ( "Found PXENV+ signature at %04x0\n", segment );
		if ( checksum ( &pxenv, sizeof ( pxenv ) ) != 0 ) {
			DBG ( "...bad checksum\n" );
			continue;
		}

		/* Allocate PXE device structure */
		pxe = malloc ( sizeof ( *pxe ) );
		if ( ! pxe ) {
			rc = -ENOMEM;
			goto err;
		}
		memset ( pxe, 0, sizeof ( *pxe ) );

		/* Add to device hierarchy */
		pxe->dev.parent = &rootdev->dev;
		INIT_LIST_HEAD ( &pxe->dev.children );
		list_add ( &pxe->dev.siblings, &rootdev->dev.children );

		/* Populate PXE device structure */
		undi_cs = pxenv.UNDICodeSeg;
		pxe->pxenv.segment = undi_cs;
		pxe->pxenv.offset = ( ( segment - undi_cs ) << 4 );
		DBGC ( pxe, "PXE %p has PXENV+ structure at %04x:%04x\n",
		       pxe, pxe->pxenv.segment, pxe->pxenv.offset );
		pxe->entry = pxenv.RMEntry;
		if ( pxenv.Version >= 0x0201 ) {
			pxe->ppxe = pxenv.PXEPtr;
			copy_from_real ( &ppxe, pxe->ppxe.segment,
					 pxe->ppxe.offset, sizeof ( ppxe ) );
			if ( ( memcmp ( ppxe.Signature, "!PXE", 4 ) == 0 ) &&
			     ( checksum ( &ppxe, sizeof ( ppxe ) ) == 0 ) ) {
				DBGC ( pxe, "PXE %p has !PXE structure at "
				       "%04x:%04x\n", pxe,
				       pxe->ppxe.segment, pxe->ppxe.offset );
				pxe->entry = ppxe.EntryPointSP;
			}
		}
		DBGC ( pxe, "PXE %p using entry point at %04x:%04x\n", pxe,
		       pxe->entry.segment, pxe->entry.offset );

		/* Register PXE device */
		if ( undi_probe ( pxe ) == 0 ) {
			/* Device registered; drop reference */
			pxe = NULL;
		} else {
			/* Not registered; re-use struct pxe_device */
			list_del ( &pxe->dev.siblings );
		}
	}

	free ( pxe );
	return 0;

 err:
	free ( pxe );
	pxebus_remove ( rootdev );
	return rc;
}

/**
 * Remove PXE root bus
 *
 * @v rootdev		PXE bus root device
 */
static void pxebus_remove ( struct root_device *rootdev ) {
	struct pxe_device *pxe;
	struct pxe_device *tmp;

	list_for_each_entry_safe ( pxe, tmp, &rootdev->dev.children,
				   dev.siblings ) {
		unregister_pxedev ( pxe );
		free ( pxe );
	}
}

/** PXE bus root device driver */
static struct root_driver pxe_root_driver = {
	.probe = pxebus_probe,
	.remove = pxebus_remove,
};

/** PXE bus root device */
struct root_device pxe_root_device __root_device = {
	.name = "PXE",
	.driver = &pxe_root_driver,
};

#endif
