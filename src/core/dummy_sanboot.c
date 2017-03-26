/*
 * Copyright (C) 2017 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Dummy SAN device
 *
 */

#include <errno.h>
#include <ipxe/sanboot.h>

/**
 * Hook dummy SAN device
 *
 * @v uri		URI
 * @v drive		Drive number
 * @ret drive		Drive number, or negative error
 */
static int dummy_san_hook ( struct uri *uri, unsigned int drive ) {
	struct san_device *sandev;
	int rc;

	/* Allocate SAN device */
	sandev = alloc_sandev ( uri, 0 );
	if ( ! sandev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	sandev->drive = drive;

	/* Register SAN device */
	if ( ( rc = register_sandev ( sandev ) ) != 0 ) {
		DBGC ( sandev, "SAN %#02x could not register: %s\n",
		       sandev->drive, strerror ( rc ) );
		goto err_register;
	}

	return drive;

	unregister_sandev ( sandev );
 err_register:
	sandev_put ( sandev );
 err_alloc:
	return rc;
}

/**
 * Unhook dummy SAN device
 *
 * @v drive		Drive number
 */
static void dummy_san_unhook ( unsigned int drive ) {
	struct san_device *sandev;

	/* Find drive */
	sandev = sandev_find ( drive );
	if ( ! sandev ) {
		DBG ( "SAN %#02x does not exist\n", drive );
		return;
	}

	/* Unregister SAN device */
	unregister_sandev ( sandev );

	/* Drop reference to drive */
	sandev_put ( sandev );
}

/**
 * Boot from dummy SAN device
 *
 * @v drive		Drive number
 */
static int dummy_san_boot ( unsigned int drive __unused ) {

	return -EOPNOTSUPP;
}

/**
 * Describe dummy SAN device
 *
 * @v drive		Drive number
 */
static int dummy_san_describe ( unsigned int drive __unused ) {

	return 0;
}

PROVIDE_SANBOOT ( dummy, san_hook, dummy_san_hook );
PROVIDE_SANBOOT ( dummy, san_unhook, dummy_san_unhook );
PROVIDE_SANBOOT ( dummy, san_boot, dummy_san_boot );
PROVIDE_SANBOOT ( dummy, san_describe, dummy_san_describe );
