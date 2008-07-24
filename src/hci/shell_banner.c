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
#include <unistd.h>
#include <config/general.h>
#include <gpxe/keys.h>
#include <gpxe/shell_banner.h>

/** @file
 *
 * Shell startup banner
 *
 */

/**
 * Print shell banner and prompt for shell entry
 *
 * @ret	enter_shell		User wants to enter shell
 */
int shell_banner ( void ) {
	int enter_shell = 0;
	int wait_count;
	int key;

	printf ( "\nPress Ctrl-B for the gPXE command line..." );

	/* Wait for key */
	for ( wait_count = 0 ; wait_count < BANNER_TIMEOUT ; wait_count++ ) {
		if ( iskey() ) {
			key = getchar();
			if ( key == CTRL_B )
				enter_shell = 1;
			break;
		}
		mdelay(100);
	}

	/* Clear the "Press Ctrl-B" line */
	printf ( "\r                                         \r" );

	return enter_shell;
}
