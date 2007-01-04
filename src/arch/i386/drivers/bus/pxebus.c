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
#include <gpxe/device.h>

/** @file
 *
 * PXE bus
 *
 */

/**
 * UNDI parameter block
 *
 * Used as the paramter block for all UNDI API calls.  Resides in base
 * memory.
 */
static union u_PXENV_ANY __data16 ( pxe_params );
#define pxe_params __use_data16 ( pxe_params )

/** UNDI entry point */
static SEGOFF16_t __data16 ( pxe_entry_point );
#define pxe_entry_point __use_data16 ( pxe_entry_point )

/**
 * Issue PXE API call
 *
 * @v pxe		PXE device
 * @v function		API call number
 * @v params		PXE parameter block
 * @v params_len	Length of PXE parameter block
 * @ret rc		Return status code
 */
int pxe_call ( struct pxe_device *pxe, unsigned int function,
	       void *params, size_t params_len ) {
	union u_PXENV_ANY *pxenv_any = params;
	PXENV_EXIT_t exit;
	int discard_b, discard_D;
	int rc;

	/* Copy parameter block and entry point */
	assert ( params_len <= sizeof ( pxe_params ) );
	memcpy ( &pxe_params, params, params_len );
	pxe_entry_point = pxe->entry;

	/* Call real-mode entry point.  This calling convention will
	 * work with both the !PXE and the PXENV+ entry points.
	 */
	__asm__ __volatile__ ( REAL_CODE ( "pushw %%es\n\t"
					   "pushw %%di\n\t"
					   "pushw %%bx\n\t"
					   "lcall *%c3\n\t"
					   "addw $6, %%sp\n\t" )
			       : "=a" ( exit ), "=b" ( discard_b ),
			         "=D" ( discard_D )
			       : "p" ( & __from_data16 ( pxe_entry_point ) ),
			         "b" ( function ),
			         "D" ( & __from_data16 ( pxe_params ) ) );

	/* UNDI API calls may rudely change the status of A20 and not
	 * bother to restore it afterwards.  Intel is known to be
	 * guilty of this.
	 *
	 * Note that we will return to this point even if A20 gets
	 * screwed up by the UNDI driver, because Etherboot always
	 * resides in an even megabyte of RAM.
	 */	
	gateA20_set();

	/* Copy parameter block back */
	memcpy ( params, &pxe_params, params_len );

	/* Determine return status code based on PXENV_EXIT and
	 * PXENV_STATUS
	 */
	if ( exit == PXENV_EXIT_SUCCESS ) {
		rc = 0;
	} else {
		rc = -pxenv_any->Status;
		/* Paranoia; don't return success for the combination
		 * of PXENV_EXIT_FAILURE but PXENV_STATUS_SUCCESS
		 */
		if ( rc == 0 )
			rc = -EIO;
	}

	return rc;
}

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
 * Get PXE device information for an instantiated device
 *
 * @v pxe		PXE device
 * @ret rc		Return status code
 */
static int pxedev_get_instance_info ( struct pxe_device *pxe ) {
	struct s_PXENV pxenv;
	struct s_PXE ppxe;
	struct s_PXENV_UNDI_GET_INFORMATION undi_info;
	int rc;

	/* Determine entry point from PXENV+ structure */
	DBGC ( pxe, "PXE %p has PXENV+ structure at %04x:%04x\n", pxe,
	      pxe->pxenv.segment, pxe->pxenv.offset );
	copy_from_real ( &pxenv, pxe->pxenv.segment, pxe->pxenv.offset,
			 sizeof ( pxenv ) );
	if ( checksum ( &pxenv, sizeof ( pxenv ) ) != 0 ) {
		DBGC ( pxe, "PXE %p bad PXENV+ checksum\n", pxe );
		return -EINVAL;
	}
	pxe->entry = pxenv.RMEntry;

	/* If API version is 2.1 or greater, use the !PXE structure instead */
	if ( pxenv.Version >= 0x0201 ) {
		pxe->ppxe = pxenv.PXEPtr;
		DBGC ( pxe, "PXE %p has !PXE structure at %04x:%04x\n", pxe,
		       pxe->ppxe.segment, pxe->ppxe.offset );
		copy_from_real ( &ppxe, pxe->ppxe.segment, pxe->ppxe.offset,
				 sizeof ( ppxe ) );
		if ( checksum ( &pxenv, sizeof ( pxenv ) ) != 0 ) {
			DBGC ( pxe, "PXE %p bad !PXE checksum\n", pxe );
			return -EINVAL;
		}
		pxe->entry = ppxe.EntryPointSP;
	}

	DBGC ( pxe, "PXE %p using entry point at %04x:%04x\n", pxe,
	       pxe->entry.segment, pxe->entry.offset );

	/* Get device information */
	memset ( &undi_info, 0, sizeof ( undi_info ) );
	if ( ( rc = pxe_call ( pxe, PXENV_UNDI_GET_INFORMATION, &undi_info,
			       sizeof ( undi_info ) ) ) != 0 ) {
		DBGC ( pxe, "PXE %p could not retrieve UNDI information: %s\n",
		       pxe, strerror ( rc ) );
		return rc;
	}
	memcpy ( pxe->hwaddr, undi_info.PermNodeAddress,
		 sizeof ( pxe->hwaddr ) );
	pxe->irq = undi_info.IntNumber;
	pxe->rom = undi_info.ROMAddress;

	return 0;
}

/**
 * Register PXE device
 *
 * @v pxe		PXE device
 * @ret rc		Return status code
 */
static int register_pxedev ( struct pxe_device *pxe ) {
	int rc;

	DBGC ( pxe, "PXE %p registering\n", pxe );

	/* Register as an UNDI driver */
	if ( ( rc = undi_probe ( pxe ) ) != 0 )
		return rc;

	/* Add to device hierarchy and return */
	list_add ( &pxe->dev.siblings, &pxe->dev.parent->children );
	return 0;
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
	struct pxe_device *pxe;
	uint16_t signature;
	uint16_t pxenv_segment;
	uint16_t pxenv_offset;
	int rc;

	/* PXE installation check */
	__asm__ __volatile__ ( REAL_CODE ( "stc\n\t"
					   "int $0x1a\n\t"
					   "jnc 1f\n\t"
					   "xorw %%ax, %%ax\n\t"
					   "\n1:\n\t"
					   "movw %%es, %%dx\n\t" )
			       : "=a" ( signature ), "=b" ( pxenv_offset ),
			         "=d" ( pxenv_segment )
			       : "a" ( 0x5650 ) );
	if ( signature != 0x564e ) {
		DBG ( "No pixies found\n" );
		return 0;
	}

	/* Allocate PXE device structure */
	pxe = malloc ( sizeof ( *pxe ) );
	if ( ! pxe ) {
		rc = -ENOMEM;
		goto err;
	}
	memset ( pxe, 0, sizeof ( *pxe ) );

	/* Populate PXE device structure */
	pxe->pxenv.segment = pxenv_segment;
	pxe->pxenv.offset = pxenv_offset;
	INIT_LIST_HEAD ( &pxe->dev.children );
	pxe->dev.parent = &rootdev->dev;
	if ( ( rc = pxedev_get_instance_info ( pxe ) ) != 0 )
		goto err;

	/* Register PXE device */
	if ( ( rc = register_pxedev ( pxe ) ) != 0 )
		goto err;

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
	.dev = {
		.children = LIST_HEAD_INIT ( pxe_root_device.dev.children ),
	},
};
