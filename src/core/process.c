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

#include <gpxe/list.h>
#include <gpxe/process.h>

/** @file
 *
 * Processes
 *
 * We implement a trivial form of cooperative multitasking, in which
 * all processes share a single stack and address space.
 */

/** Process run queue */
static LIST_HEAD ( run_queue );

/**
 * Add process to run queue
 *
 * @v process		Process
 */
void schedule ( struct process *process ) {
	list_add_tail ( &process->list, &run_queue );
}

/**
 * Single-step a single process
 *
 * This removes the first process from the run queue and executes a
 * single step of that process.
 */
void step ( void ) {
	struct process *process;

	list_for_each_entry ( process, &run_queue, list ) {
		list_del ( &process->list );
		process->step ( process );
		break;
	}
}
