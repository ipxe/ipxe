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

#include <stdio.h>
#include <console.h>
#include <gpxe/timer.h>
#include <gpxe/shell_banner.h>

/** @file
 *
 * Shell startup banner
 *
 */

#define BANNER_TIMEOUT ( 2 * TICKS_PER_SEC )

/**
 * Print shell banner and prompt for shell entry
 *
 * @ret	enter_shell		User wants to enter shell
 */
int shell_banner ( void ) {
	unsigned long timeout = ( currticks() + BANNER_TIMEOUT );
	int enter_shell = 0;
	int key;

	printf ( "\nPress Ctrl-B for the gPXE command line..." );

	/* Wait for key */
	while ( currticks() < timeout ) {
		if ( iskey() ) {
			key = getchar();
			if ( key == 0x02 /* Ctrl-B */ )
				enter_shell = 1;
			break;
		}
	}

	/* Clear the "Press Ctrl-B" line */
	printf ( "\r                                         \r" );

	return enter_shell;
}
