/*
 * Copyright (C) 2018 Michael Brown <mbrown@fensystems.co.uk>.
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
 * NetBIOS user names
 *
 */

#include <stddef.h>
#include <string.h>
#include <ipxe/netbios.h>

/**
 * Split NetBIOS [domain\]username into separate domain and username fields
 *
 * @v username		NetBIOS [domain\]username string
 * @ret domain		Domain portion of string, or NULL if no domain present
 *
 * This function modifies the original string by removing the
 * separator.  The caller may restore the string using
 * netbios_domain_undo().
 */
const char * netbios_domain ( char **username ) {
	char *domain_username = *username;
	char *sep;

	/* Find separator, if present */
	sep = strchr ( domain_username, '\\' );
	if ( ! sep )
		return NULL;

	/* Overwrite separator with NUL terminator and update username string */
	*sep = '\0';
	*username = ( sep + 1 );

	return domain_username;
}
