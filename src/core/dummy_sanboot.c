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

#include <string.h>
#include <errno.h>
#include <ipxe/sanboot.h>

/**
 * Hook dummy SAN device
 *
 * @v drive		Drive number
 * @v uris		List of URIs
 * @v count		Number of URIs
 * @v flags		Flags
 * @ret drive		Drive number, or negative error
 */
static int dummy_san_hook ( unsigned int drive, struct uri **uris,
			    unsigned int count, unsigned int flags ) {
	struct san_device *sandev;
	int rc;

	/* Allocate SAN device */
	sandev = alloc_sandev ( uris, count, 0 );
	if ( ! sandev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Register SAN device */
	if ( ( rc = register_sandev ( sandev, drive, flags ) ) != 0 ) {
		DBGC ( sandev->drive, "SAN %#02x could not register: %s\n",
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
		DBGC ( drive, "SAN %#02x does not exist\n", drive );
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
 * @v config		Boot configuration parameters
 * @ret rc		Return status code
 */
static int dummy_san_boot ( unsigned int drive __unused,
			    struct san_boot_config *config __unused ) {

	return -EOPNOTSUPP;
}

/**
 * Install ACPI table
 *
 * @v acpi		ACPI description header
 * @ret rc		Return status code
 */
static int dummy_install ( struct acpi_header *acpi ) {

	DBGC ( acpi, "ACPI table %s:\n", acpi_name ( acpi->signature ) );
	DBGC_HDA ( acpi, 0, acpi, le32_to_cpu ( acpi->length ) );
	return 0;
}

/**
 * Describe dummy SAN device
 *
 * @ret rc		Return status code
 */
static int dummy_san_describe ( void ) {

	return acpi_install ( dummy_install );
}

PROVIDE_SANBOOT ( dummy, san_hook, dummy_san_hook );
PROVIDE_SANBOOT ( dummy, san_unhook, dummy_san_unhook );
PROVIDE_SANBOOT ( dummy, san_boot, dummy_san_boot );
PROVIDE_SANBOOT ( dummy, san_describe, dummy_san_describe );
