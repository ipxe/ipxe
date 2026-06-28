/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Installed facilities
 *
 */

#include <string.h>
#include <ipxe/facility.h>

/**
 * Check if facility is installed
 *
 * @v facility		Facility ID
 * @ret is_installed	Facility is installed
 */
int facility_is_installed ( unsigned int facility ) {
	struct s390x_facilities facilities;
	register unsigned long max asm ( "0" );

	/* Get installed facilities */
	memset ( &facilities, 0, sizeof ( facilities ) );
	max = ( ( sizeof ( facilities.mask ) /
		  sizeof ( facilities.mask[0] ) ) - 1 );
	__asm__ ( "stfle %0" : "=R" ( facilities ), "+r" ( max ) );
	DBGC ( &facilities, "FACILITY %016llx:%016llx:%016llx:%016llx\n",
	       facilities.mask[0], facilities.mask[1], facilities.mask[2],
	       facilities.mask[3] );

	return ( !! ( facilities.mask[ facility / 64 ] &
		      ( 1UL << ( ~facility % 64 ) ) ) );
}
