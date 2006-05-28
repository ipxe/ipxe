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

#include <stddef.h>
#include <gpxe/aoe.h>

/** @file
 *
 * AoE ATA device
 *
 */

/**
 * Issue ATA command via AoE device
 *
 * @v ata		ATA device
 * @v command		ATA command
 * @ret rc		Return status code
 */
static int aoe_command ( struct ata_device *ata,
			 struct ata_command *command ) {
	struct aoe_device *aoedev
		= container_of ( ata, struct aoe_device, ata );

	return aoe_issue ( &aoedev->aoe, command );
}

/**
 * Initialise AoE device
 *
 * @v aoedev		AoE device
 */
int init_aoedev ( struct aoe_device *aoedev ) {
	aoedev->ata.command = aoe_command;
	aoe_open ( &aoedev->aoe );
	return init_atadev ( &aoedev->ata );
}
