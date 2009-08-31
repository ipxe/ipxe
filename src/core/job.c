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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <string.h>
#include <errno.h>
#include <gpxe/job.h>

/** @file
 *
 * Job control interfaces
 *
 */

void job_done ( struct job_interface *job, int rc ) {
	struct job_interface *dest = job_get_dest ( job );

	job_unplug ( job );
	dest->op->done ( dest, rc );
	job_put ( dest );
}

void job_kill ( struct job_interface *job ) {
	struct job_interface *dest = job_get_dest ( job );

	job_unplug ( job );
	dest->op->kill ( dest );
	job_put ( dest );
}

void job_progress ( struct job_interface *job,
		    struct job_progress *progress ) {
	struct job_interface *dest = job_get_dest ( job );

	dest->op->progress ( dest, progress );
	job_put ( dest );
}

/****************************************************************************
 *
 * Helper methods
 *
 * These functions are designed to be used as methods in the
 * job_interface_operations table.
 *
 */

void ignore_job_done ( struct job_interface *job __unused, int rc __unused ) {
	/* Nothing to do */
}

void ignore_job_kill ( struct job_interface *job __unused ) {
	/* Nothing to do */
}

void ignore_job_progress ( struct job_interface *job __unused,
			   struct job_progress *progress ) {
	memset ( progress, 0, sizeof ( *progress ) );
}

/** Null job control interface operations */
struct job_interface_operations null_job_ops = {
	.done		= ignore_job_done,
	.kill		= ignore_job_kill,
	.progress	= ignore_job_progress,
};

/**
 * Null job control interface
 *
 * This is the interface to which job control interfaces are connected
 * when unplugged.  It will never generate messages, and will silently
 * absorb all received messages.
 */
struct job_interface null_job = {
	.intf = {
		.dest = &null_job.intf,
		.refcnt = NULL,
	},
	.op = &null_job_ops,
};
