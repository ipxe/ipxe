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

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <gpxe/netdevice.h>
#include <gpxe/dhcp.h>
#include <gpxe/image.h>
#include <usr/ifmgmt.h>
#include <usr/route.h>
#include <usr/dhcpmgmt.h>
#include <usr/imgmgmt.h>
#include <usr/iscsiboot.h>
#include <usr/aoeboot.h>
#include <usr/autoboot.h>

/** @file
 *
 * Automatic booting
 *
 */

/**
 * Identify the boot network device
 *
 * @ret netdev		Boot network device
 */
static struct net_device * find_boot_netdev ( void ) {
	return NULL;
}

/**
 * Boot using filename
 *
 * @v filename		Boot filename
 * @ret rc		Return status code
 */
static int boot_filename ( const char *filename ) {
	struct image *image;
	int rc;

	image = alloc_image();
	if ( ! image ) {
		printf ( "Out of memory\n" );
		return -ENOMEM;
	}
	if ( ( rc = imgfetch ( image, filename,
			       register_and_autoexec_image ) ) != 0 ) {
		printf ( "Could not retrieve %s: %s\n",
			 filename, strerror ( rc ) );
		image_put ( image );
		return rc;
	}

	image_put ( image );
	return 0;
}

/**
 * Boot using root path
 *
 * @v root_path		Root path
 * @ret rc		Return status code
 */
static int boot_root_path ( const char *root_path ) {

	/* Quick hack */
	if ( strncmp ( root_path, "iscsi:", 6 ) == 0 ) {
		return iscsiboot ( root_path );
	} else if ( strncmp ( root_path, "aoe:", 4 ) == 0 ) {
		return aoeboot ( root_path );
	}

	return -ENOTSUP;
}

/**
 * Boot from a network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int netboot ( struct net_device *netdev ) {
	char buf[256];
	int rc;

	/* Open device and display device status */
	if ( ( rc = ifopen ( netdev ) ) != 0 )
		return rc;
	ifstat ( netdev );

	/* Configure device via DHCP */
	if ( ( rc = dhcp ( netdev ) ) != 0 )
		return rc;
	route();

	/* Try to download and boot whatever we are given as a filename */
	dhcp_snprintf ( buf, sizeof ( buf ),
			find_global_dhcp_option ( DHCP_BOOTFILE_NAME ) );
	if ( buf[0] ) {
		printf ( "Booting from filename \"%s\"\n", buf );
		return boot_filename ( buf );
	}
	
	/* No filename; try the root path */
	dhcp_snprintf ( buf, sizeof ( buf ),
			find_global_dhcp_option ( DHCP_ROOT_PATH ) );
	if ( buf[0] ) {
		printf ( "Booting from root path \"%s\"\n", buf );
		return boot_root_path ( buf );
	}

	printf ( "No filename or root path specified\n" );
	return -ENOENT;
}

/**
 * Close all open net devices
 *
 * Called before a fresh boot attempt in order to free up memory.  We
 * don't just close the device immediately after the boot fails,
 * because there may still be TCP connections in the process of
 * closing.
 */
static void close_all_netdevs ( void ) {
	struct net_device *netdev;

	for_each_netdev ( netdev ) {
		ifclose ( netdev );
	}
}

/**
 * Boot the system
 */
void autoboot ( void ) {
	struct net_device *boot_netdev;
	struct net_device *netdev;

	/* If we have an identifable boot device, try that first */
	close_all_netdevs();
	if ( ( boot_netdev = find_boot_netdev() ) )
		netboot ( boot_netdev );

	/* If that fails, try booting from any of the other devices */
	for_each_netdev ( netdev ) {
		if ( netdev == boot_netdev )
			continue;
		close_all_netdevs();
		netboot ( netdev );
	}

	printf ( "No more network devices\n" );
}
