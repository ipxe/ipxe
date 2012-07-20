/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ipxe/process.h>
#include <ipxe/console.h>
#include <ipxe/keys.h>
#include <ipxe/job.h>
#include <ipxe/monojob.h>
#include <ipxe/timer.h>

/** @file
 *
 * Single foreground job
 *
 */

static int monojob_rc;

static void monojob_close ( struct interface *intf, int rc ) {
	monojob_rc = rc;
	intf_restart ( intf, rc );
}

static struct interface_operation monojob_intf_op[] = {
	INTF_OP ( intf_close, struct interface *, monojob_close ),
};

static struct interface_descriptor monojob_intf_desc =
	INTF_DESC_PURE ( monojob_intf_op );

struct interface monojob = INTF_INIT ( monojob_intf_desc );

/**
 * Wait for single foreground job to complete
 *
 * @v string		Job description to display, or NULL to be silent
 * @ret rc		Job final status code
 */
int monojob_wait ( const char *string ) {
	struct job_progress progress;
	int key;
	int rc;
	unsigned long last_keycheck;
	unsigned long last_progress;
	unsigned long now;
	unsigned long elapsed;
	unsigned long completed;
	unsigned long total;
	unsigned int percentage;
	int shown_percentage = 0;

	if ( string )
		printf ( "%s...", string );
	monojob_rc = -EINPROGRESS;
	last_keycheck = last_progress = currticks();
	while ( monojob_rc == -EINPROGRESS ) {

		/* Allow job to progress */
		step();
		now = currticks();

		/* Check for keypresses.  This can be time-consuming,
		 * so check only once per clock tick.
		 */
		if ( now != last_keycheck ) {
			if ( iskey() ) {
				key = getchar();
				switch ( key ) {
				case CTRL_C:
					monojob_close ( &monojob, -ECANCELED );
					break;
				default:
					break;
				}
			}
			last_keycheck = now;
		}

		/* Display progress, if applicable */
		elapsed = ( now - last_progress );
		if ( string && ( elapsed >= TICKS_PER_SEC ) ) {
			if ( shown_percentage )
				printf ( "\b\b\b\b    \b\b\b\b" );
			job_progress ( &monojob, &progress );
			/* Normalise progress figures to avoid overflow */
			completed = ( progress.completed / 128 );
			total = ( progress.total / 128 );
			if ( total ) {
				percentage = ( ( 100 * completed ) / total );
				printf ( "%3d%%", percentage );
				shown_percentage = 1;
			} else {
				printf ( "." );
				shown_percentage = 0;
			}
			last_progress = now;
		}
	}
	rc = monojob_rc;

	if ( shown_percentage )
		printf ( "\b\b\b\b    \b\b\b\b" );

	if ( string ) {
		if ( rc ) {
			printf ( " %s\n", strerror ( rc ) );
		} else {
			printf ( " ok\n" );
		}
	}

	return rc;
}
