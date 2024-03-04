/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <string.h>
#include <errno.h>
#include <ipxe/uri.h>
#include <ipxe/settings.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_path.h>
#include <ipxe/efi/efi_uripath.h>


/** @file
 *
 * EFI uri source
 *
 */

/**
 * Identify uri source 
 *
 * @v device		Device handle
 * @v path		Device path
 * @ret rc		Return status code
 */
int efi_set_uri_path ( EFI_HANDLE device,
			       EFI_DEVICE_PATH_PROTOCOL *path ) {

	EFI_DEVICE_PATH_PROTOCOL * next;
	URI_DEVICE_PATH * uri;
	size_t uri_len;
	int rc;

	/* Scan for uri device path */
	for ( ; ( next = efi_path_next ( path ) ) ; path = next ) {
		if ( ( path->Type == MESSAGING_DEVICE_PATH ) &&
		     ( path->SubType == MSG_URI_DP ) ) {
			uri = container_of ( path, URI_DEVICE_PATH, Header );
			uri_len = strlen ( uri->Uri ) ;
			DBGC ( device , "EFI found URI Device Path [%s] %ld\n",
				uri->Uri, uri_len );

			/* Store in 'uri-src-path' setting */
			if ( (rc = store_setting ( NULL, &uri_path_setting ,
				uri->Uri , uri_len ) ) != 0 ) {
				DBGC ( device, "Unable to save URI path into settings %s\n" ,
					strerror ( rc ));
				return rc;
			}
		}
	}

	return 0;
}
