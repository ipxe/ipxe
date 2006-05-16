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

#include <gpxe/list.h>
#include <gpxe/tables.h>
#include <gpxe/device.h>

/**
 * @file
 *
 * Device model
 *
 */

static struct root_device root_devices[0] __table_start ( root_devices );
static struct root_device root_devices_end[0] __table_end ( root_devices );

/** Registered root devices */
static LIST_HEAD ( devices );

/**
 * Register a root device
 *
 * @v rootdev		Root device
 * @ret rc		Return status code
 *
 * Calls the root device driver's probe() method, and adds it to the
 * list of registered root devices if successful.
 */
static int register_rootdev ( struct root_device *rootdev ) {
	int rc;

	DBG ( "Registering %s root bus\n", rootdev->name );

	if ( ( rc = rootdev->driver->probe ( rootdev ) ) != 0 )
		return rc;

	list_add ( &rootdev->dev.siblings, &devices );
	return 0;
}

/**
 * Unregister a root device
 *
 * @v rootdev		Root device
 */
static void unregister_rootdev ( struct root_device *rootdev ) {
	rootdev->driver->remove ( rootdev );
	list_del ( &rootdev->dev.siblings );
	DBG ( "Unregistered %s root bus\n", rootdev->name );
}

/**
 * Probe all devices
 *
 * @ret rc		Return status code
 *
 * This initiates probing for all devices in the system.  After this
 * call, the device hierarchy will be populated, and all hardware
 * should be ready to use.
 */
int probe_devices ( void ) {
	struct root_device *rootdev;

	for ( rootdev = root_devices; rootdev < root_devices_end; rootdev++ ) {
		register_rootdev ( rootdev );
	}
	return 0;
}

/**
 * Remove all devices
 *
 */
void remove_devices ( void ) {
	struct root_device *rootdev;
	struct root_device *tmp;

	list_for_each_entry_safe ( rootdev, tmp, &devices, dev.siblings ) {
		unregister_rootdev ( rootdev );
	}
}
