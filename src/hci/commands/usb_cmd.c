/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <ipxe/usb.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * USB commands
 *
 */

/** "usbscan" options */
struct usbscan_options {};

/** "usbscan" option list */
static struct option_descriptor usbscan_opts[] = {};

/** "usbscan" command descriptor */
static struct command_descriptor usbscan_cmd =
	COMMAND_DESC ( struct usbscan_options, usbscan_opts, 1, 1,
		       "<setting>" );

/**
 * "usbscan" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int usbscan_exec ( int argc, char **argv ) {
	struct usbscan_options opts;
	struct named_setting setting;
	struct usb_device *usb;
	unsigned long prev;
	uint16_t busdev;
	int len;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &usbscan_cmd, &opts ) ) != 0 )
		goto err_parse_options;

	/* Parse setting name */
	if ( ( rc = parse_autovivified_setting ( argv[optind],
						 &setting ) ) != 0 )
		goto err_parse_setting;

	/* Determine starting bus:dev.fn address */
	if ( ( len = fetchn_setting ( setting.settings, &setting.setting,
				      NULL, &setting.setting, &prev ) ) < 0 ) {
		/* Setting not yet defined: start searching from 00:00 */
		busdev = 0;
	} else {
		/* Setting is defined: start searching from next location */
		busdev = ( prev + 1 );
		if ( ! busdev ) {
			rc = -ENOENT;
			goto err_end;
		}
	}

	/* Find next existent USB device */
	if ( ( rc = usb_find_next ( &usb, &busdev ) ) != 0 )
		goto err_find_next;

	/* Apply default type if necessary.  Use ":uint16" rather than
	 * ":hex" to allow for easy inclusion within a
	 * "${usb/${location}....}" constructed setting.
	 */
	if ( ! setting.setting.type )
		setting.setting.type = &setting_type_uint16;

	/* Store setting */
	if ( ( rc = storen_setting ( setting.settings, &setting.setting,
				     busdev ) ) != 0 ) {
		printf ( "Could not store \"%s\": %s\n",
			 setting.setting.name, strerror ( rc ) );
		goto err_store;
	}

 err_store:
 err_end:
 err_find_next:
 err_parse_setting:
 err_parse_options:
	return rc;
}

/** USB commands */
struct command usb_commands[] __command = {
	{
		.name = "usbscan",
		.exec = usbscan_exec,
	},
};
