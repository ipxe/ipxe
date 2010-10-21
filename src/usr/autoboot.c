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
#include <ipxe/netdevice.h>
#include <ipxe/dhcp.h>
#include <ipxe/settings.h>
#include <ipxe/image.h>
#include <ipxe/sanboot.h>
#include <ipxe/uri.h>
#include <ipxe/init.h>
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
 * Perform PXE menu boot when PXE stack is not available
 */
__weak int pxe_menu_boot ( struct net_device *netdev __unused ) {
	return -ENOTSUP;
}

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
	if ( ! uri ) {
		printf ( "Could not parse \"%s\"\n", filename );
		rc = -ENOMEM;
		goto err_parse_uri;
	}
	filename_is_absolute = uri_is_absolute ( uri );
	uri_put ( uri );

	/* Construct a tftp:// URI for the filename, if applicable.
	 * We can't just rely on the current working URI, because the
	 * relative URI resolution will remove the distinction between
	 * filenames with and without initial slashes, which is
	 * significant for TFTP.
	 */
	if ( ! filename_is_absolute ) {
		snprintf ( buf, sizeof ( buf ), "tftp://%s/",
			   inet_ntoa ( next_server ) );
		uri_encode ( filename, buf + strlen ( buf ),
			     sizeof ( buf ) - strlen ( buf ), URI_PATH );
		filename = buf;
	}

	/* Download and boot image */
	image = alloc_image();
	if ( ! image ) {
		printf ( "Could not allocate image\n" );
		rc = -ENOMEM;
		goto err_alloc_image;
	}
	if ( ( rc = imgfetch ( image, filename,
			       register_and_autoload_image ) ) != 0 ) {
		printf ( "Could not fetch image: %s\n", strerror ( rc ) );
		goto err_imgfetch;
	}
	if ( ( rc = imgexec ( image ) ) != 0 ) {
		printf ( "Could not execute image: %s\n", strerror ( rc ) );
		goto err_imgexec;
	}

	/* Drop image reference */
	image_put ( image );
	return 0;

 err_imgexec:
 err_imgfetch:
	image_put ( image );
 err_alloc_image:
 err_parse_uri:
	return rc;
}

/** The "keep-san" setting */
struct setting keep_san_setting __setting = {
	.name = "keep-san",
	.description = "Preserve SAN connection",
	.tag = DHCP_EB_KEEP_SAN,
	.type = &setting_type_int8,
};

/** The "skip-san-boot" setting */
struct setting skip_san_boot_setting __setting = {
	.name = "skip-san-boot",
	.description = "Do not boot the SAN drive after connecting",
	.tag = DHCP_EB_SKIP_SAN_BOOT,
	.type = &setting_type_int8,
};

/**
 * Boot using root path
 *
 * @v root_path		Root path
 * @ret rc		Return status code
 */
int boot_root_path ( const char *root_path ) {
	struct uri *uri;
	int drive;
	int rc;

	/* Parse URI */
	uri = parse_uri ( root_path );
	if ( ! uri ) {
		printf ( "Could not parse \"%s\"\n", root_path );
		rc = -ENOMEM;
		goto err_parse_uri;
	}

	/* Hook SAN device */
	if ( ( drive = san_hook ( uri, 0 ) ) < 0 ) {
		rc = drive;
		printf ( "Could not open SAN device: %s\n",
			 strerror ( rc ) );
		goto err_open;
	}
	printf ( "Registered as SAN device %#02x\n", drive );

	/* Describe SAN device */
	if ( ( rc = san_describe ( drive ) ) != 0 ) {
		printf ( "Could not describe SAN device %#02x: %s\n",
			 drive, strerror ( rc ) );
		goto err_describe;
	}

	/* Boot from SAN device */
	if ( fetch_intz_setting ( NULL, &skip_san_boot_setting) != 0 ) {
		printf ( "Skipping boot from SAN device %#02x\n", drive );
	} else {
		printf ( "Booting from SAN device %#02x\n", drive );
		rc = san_boot ( drive );
		printf ( "Boot from SAN device %#02x failed: %s\n",
			 drive, strerror ( rc ) );
	}

	/* Leave drive registered, if instructed to do so */
	if ( fetch_intz_setting ( NULL, &keep_san_setting ) != 0 ) {
		printf ( "Preserving connection to SAN device %#02x\n",
			 drive );
		shutdown_exit_flags |= SHUTDOWN_KEEP_DEVICES;
		goto err_keep_san;
	}

	/* Unhook SAN deivce */
	printf ( "Unregistering SAN device %#02x\n", drive );
	san_unhook ( drive );

	/* Drop URI reference */
	uri_put ( uri );

	return 0;

 err_keep_san:
 err_describe:
 err_open:
	uri_put ( uri );
 err_parse_uri:
	return rc;
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
	if ( ( strcmp ( buf, "PXEClient" ) == 0 ) &&
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
		return boot_next_server_and_filename ( next_server, buf );
	}
	
	/* No filename; try the root path */
	fetch_string_setting ( NULL, &root_path_setting, buf, sizeof ( buf ) );
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
