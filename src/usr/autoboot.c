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
#include <stdio.h>
#include <errno.h>
#include <gpxe/netdevice.h>
#include <gpxe/dhcp.h>
#include <gpxe/settings.h>
#include <gpxe/image.h>
#include <gpxe/sanboot.h>
#include <gpxe/uri.h>
#include <usr/ifmgmt.h>
#include <usr/route.h>
#include <usr/dhcpmgmt.h>
#include <usr/imgmgmt.h>
#include <usr/autoboot.h>

/** @file
 *
 * Automatic booting
 *
 */

/** Shutdown flags for exit */
int shutdown_exit_flags = 0;

/**
 * Identify the boot network device
 *
 * @ret netdev		Boot network device
 */
static struct net_device * find_boot_netdev ( void ) {
	return NULL;
}

/**
 * Boot using next-server and filename
 *
 * @v filename		Boot filename
 * @ret rc		Return status code
 */
int boot_next_server_and_filename ( struct in_addr next_server,
				    const char *filename ) {
	struct uri *uri;
	struct image *image;
	char buf[ 23 /* tftp://xxx.xxx.xxx.xxx/ */ +
		  ( 3 * strlen(filename) ) /* completely URI-encoded */
		  + 1 /* NUL */ ];
	int filename_is_absolute;
	int rc;

	/* Construct URI */
	uri = parse_uri ( filename );
	if ( ! uri )
		return -ENOMEM;
	filename_is_absolute = uri_is_absolute ( uri );
	uri_put ( uri );
	if ( ! filename_is_absolute ) {
		/* Construct a tftp:// URI for the filename.  We can't
		 * just rely on the current working URI, because the
		 * relative URI resolution will remove the distinction
		 * between filenames with and without initial slashes,
		 * which is significant for TFTP.
		 */
		snprintf ( buf, sizeof ( buf ), "tftp://%s/",
			   inet_ntoa ( next_server ) );
		uri_encode ( filename, buf + strlen ( buf ),
			     sizeof ( buf ) - strlen ( buf ), URI_PATH );
		filename = buf;
	}

	image = alloc_image();
	if ( ! image )
		return -ENOMEM;
	if ( ( rc = imgfetch ( image, filename,
			       register_and_autoload_image ) ) != 0 ) {
		goto done;
	}
	if ( ( rc = imgexec ( image ) ) != 0 )
		goto done;

 done:
	image_put ( image );
	return rc;
}

/**
 * Boot using root path
 *
 * @v root_path		Root path
 * @ret rc		Return status code
 */
int boot_root_path ( const char *root_path ) {
	struct sanboot_protocol *sanboot;

	/* Quick hack */
	for_each_table_entry ( sanboot, SANBOOT_PROTOCOLS ) {
		if ( strncmp ( root_path, sanboot->prefix,
			       strlen ( sanboot->prefix ) ) == 0 ) {
			return sanboot->boot ( root_path );
		}
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
	struct setting vendor_class_id_setting
		= { .tag = DHCP_VENDOR_CLASS_ID };
	struct setting pxe_discovery_control_setting
		= { .tag = DHCP_PXE_DISCOVERY_CONTROL };
	struct setting pxe_boot_menu_setting
		= { .tag = DHCP_PXE_BOOT_MENU };
	char buf[256];
	struct in_addr next_server;
	unsigned int pxe_discovery_control;
	int rc;

	/* Open device and display device status */
	if ( ( rc = ifopen ( netdev ) ) != 0 )
		return rc;
	ifstat ( netdev );

	/* Configure device via DHCP */
	if ( ( rc = dhcp ( netdev ) ) != 0 )
		return rc;
	route();

	/* Try PXE menu boot, if applicable */
	fetch_string_setting ( NULL, &vendor_class_id_setting,
			       buf, sizeof ( buf ) );
	pxe_discovery_control =
		fetch_uintz_setting ( NULL, &pxe_discovery_control_setting );
	if ( ( strcmp ( buf, "PXEClient" ) == 0 ) && pxe_menu_boot != NULL &&
	     setting_exists ( NULL, &pxe_boot_menu_setting ) &&
	     ( ! ( ( pxe_discovery_control & PXEBS_SKIP ) &&
		   setting_exists ( NULL, &filename_setting ) ) ) ) {
		printf ( "Booting from PXE menu\n" );
		return pxe_menu_boot ( netdev );
	}

	/* Try to download and boot whatever we are given as a filename */
	fetch_ipv4_setting ( NULL, &next_server_setting, &next_server );
	fetch_string_setting ( NULL, &filename_setting, buf, sizeof ( buf ) );
	if ( buf[0] ) {
		printf ( "Booting from filename \"%s\"\n", buf );
		if ( ( rc = boot_next_server_and_filename ( next_server,
							    buf ) ) != 0 ) {
			printf ( "Could not boot from filename \"%s\": %s\n",
				 buf, strerror ( rc ) );
			return rc;
		}
		return 0;
	}
	
	/* No filename; try the root path */
	fetch_string_setting ( NULL, &root_path_setting, buf, sizeof ( buf ) );
	if ( buf[0] ) {
		printf ( "Booting from root path \"%s\"\n", buf );
		if ( ( rc = boot_root_path ( buf ) ) != 0 ) {
			printf ( "Could not boot from root path \"%s\": %s\n",
				 buf, strerror ( rc ) );
			return rc;
		}
		return 0;
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
