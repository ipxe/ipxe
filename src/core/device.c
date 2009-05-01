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

FILE_LICENCE ( GPL2_OR_LATER );

#include <string.h>
#include <gpxe/list.h>
#include <gpxe/tables.h>
#include <gpxe/device.h>
#include <gpxe/init.h>

/**
 * @file
 *
 * Device model
 *
 */

/** Registered root devices */
static LIST_HEAD ( devices );

/**
 * Probe a root device
 *
 * @v rootdev		Root device
 * @ret rc		Return status code
 */
static int rootdev_probe ( struct root_device *rootdev ) {
	int rc;

	DBG ( "Adding %s root bus\n", rootdev->dev.name );
	if ( ( rc = rootdev->driver->probe ( rootdev ) ) != 0 ) {
		DBG ( "Failed to add %s root bus: %s\n",
		      rootdev->dev.name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Remove a root device
 *
 * @v rootdev		Root device
 */
static void rootdev_remove ( struct root_device *rootdev ) {
	rootdev->driver->remove ( rootdev );
	DBG ( "Removed %s root bus\n", rootdev->dev.name );
}

/**
 * Probe all devices
 *
 * This initiates probing for all devices in the system.  After this
 * call, the device hierarchy will be populated, and all hardware
 * should be ready to use.
 */
static void probe_devices ( void ) {
	struct root_device *rootdev;
	int rc;

	for_each_table_entry ( rootdev, ROOT_DEVICES ) {
		list_add ( &rootdev->dev.siblings, &devices );
		INIT_LIST_HEAD ( &rootdev->dev.children );
		if ( ( rc = rootdev_probe ( rootdev ) ) != 0 )
			list_del ( &rootdev->dev.siblings );
	}
}

/**
 * Remove all devices
 *
 */
static void remove_devices ( int flags ) {
	struct root_device *rootdev;
	struct root_device *tmp;

	if ( flags & SHUTDOWN_KEEP_DEVICES ) {
		DBG ( "Refusing to remove devices on shutdown\n" );
		return;
	}

	list_for_each_entry_safe ( rootdev, tmp, &devices, dev.siblings ) {
		rootdev_remove ( rootdev );
		list_del ( &rootdev->dev.siblings );
	}
}

struct startup_fn startup_devices __startup_fn ( STARTUP_NORMAL ) = {
	.startup = probe_devices,
	.shutdown = remove_devices,
};
