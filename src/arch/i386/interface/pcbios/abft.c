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

FILE_LICENCE ( GPL2_OR_LATER );

#include <realmode.h>
#include <gpxe/aoe.h>
#include <gpxe/netdevice.h>
#include <gpxe/abft.h>

/** @file
 *
 * AoE Boot Firmware Table
 *
 */

#define abftab __use_data16 ( abftab )
/** The aBFT used by gPXE */
struct abft_table __data16 ( abftab ) __attribute__ (( aligned ( 16 ) )) = {
	/* ACPI header */
	.acpi = {
		.signature = ABFT_SIG,
		.length = sizeof ( abftab ),
		.revision = 1,
		.oem_id = "FENSYS",
		.oem_table_id = "gPXE",
	},
};

/**
 * Fill in all variable portions of aBFT
 *
 * @v aoe		AoE session
 */
void abft_fill_data ( struct aoe_session *aoe ) {

	/* Fill in boot parameters */
	abftab.shelf = aoe->major;
	abftab.slot = aoe->minor;
	memcpy ( abftab.mac, aoe->netdev->ll_addr, sizeof ( abftab.mac ) );

	/* Update checksum */
	acpi_fix_checksum ( &abftab.acpi );

	DBG ( "AoE boot firmware table:\n" );
	DBG_HD ( &abftab, sizeof ( abftab ) );
}
