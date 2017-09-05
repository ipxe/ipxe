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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

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
 * Clear previously displayed message
 *
 * @v len		Length of previously displayed message
 */
static void monojob_clear ( size_t len ) {
	unsigned int i;

	for ( i = 0 ; i < len ; i++ )
		putchar ( '\b' );
	for ( i = 0 ; i < len ; i++ )
		putchar ( ' ' );
	for ( i = 0 ; i < len ; i++ )
		putchar ( '\b' );
}

/**
 * Wait for single foreground job to complete
 *
 * @v string		Job description to display, or NULL to be silent
 * @v timeout		Timeout period, in ticks (0=indefinite)
 * @ret rc		Job final status code
 */
int monojob_wait ( const char *string, unsigned long timeout ) {
	struct job_progress progress;
	unsigned long last_check;
	unsigned long last_progress;
	unsigned long last_display;
	unsigned long now;
	unsigned long elapsed;
	unsigned long completed = 0;
	unsigned long scaled_completed;
	unsigned long scaled_total;
	unsigned int percentage;
	size_t clear_len = 0;
	int ongoing_rc;
	int key;
	int rc;

	if ( string )
		printf ( "%s...", string );
	monojob_rc = -EINPROGRESS;
	last_check = last_progress = last_display = currticks();
	while ( monojob_rc == -EINPROGRESS ) {

		/* Allow job to progress */
		step();
		now = currticks();

		/* Continue until a timer tick occurs (to minimise
		 * time wasted checking for progress and keypresses).
		 */
		elapsed = ( now - last_check );
		if ( ! elapsed )
			continue;
		last_check = now;

		/* Check for keypresses */
		if ( iskey() ) {
			key = getchar();
			if ( key == CTRL_C ) {
				monojob_rc = -ECANCELED;
				break;
			}
		}

		/* Monitor progress */
		ongoing_rc = job_progress ( &monojob, &progress );

		/* Reset timeout if progress has been made */
		if ( completed != progress.completed )
			last_progress = now;
		completed = progress.completed;

		/* Check for timeout, if applicable */
		elapsed = ( now - last_progress );
		if ( timeout && ( elapsed >= timeout ) ) {
			monojob_rc = ( ongoing_rc ? ongoing_rc : -ETIMEDOUT );
			break;
		}

		/* Display progress, if applicable */
		elapsed = ( now - last_display );
		if ( string && ( elapsed >= TICKS_PER_SEC ) ) {
			monojob_clear ( clear_len );
			/* Normalise progress figures to avoid overflow */
			scaled_completed = ( progress.completed / 128 );
			scaled_total = ( progress.total / 128 );
			if ( scaled_total ) {
				percentage = ( ( 100 * scaled_completed ) /
					       scaled_total );
				clear_len = printf ( "%3d%%", percentage );
			} else {
				printf ( "." );
				clear_len = 0;
			}
			if ( progress.message[0] ) {
				clear_len += printf ( " [%s]",
						      progress.message );
			}
			last_display = now;
		}
	}
	rc = monojob_rc;
	monojob_close ( &monojob, rc );

	monojob_clear ( clear_len );
	if ( string ) {
		if ( rc ) {
			printf ( " %s\n", strerror ( rc ) );
		} else {
			printf ( " ok\n" );
		}
	}

	return rc;
}
