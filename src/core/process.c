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

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/list.h>
#include <gpxe/init.h>
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
 * Add process to process list
 *
 * @v process		Process
 *
 * It is safe to call process_add() multiple times; further calls will
 * have no effect.
 */
void process_add ( struct process *process ) {
	if ( list_empty ( &process->list ) ) {
		DBGC ( process, "PROCESS %p starting\n", process );
		ref_get ( process->refcnt );
		list_add_tail ( &process->list, &run_queue );
	} else {
		DBGC ( process, "PROCESS %p already started\n", process );
	}
}

/**
 * Remove process from process list
 *
 * @v process		Process
 *
 * It is safe to call process_del() multiple times; further calls will
 * have no effect.
 */
void process_del ( struct process *process ) {
	if ( ! list_empty ( &process->list ) ) {
		DBGC ( process, "PROCESS %p stopping\n", process );
		list_del ( &process->list );
		INIT_LIST_HEAD ( &process->list );
		ref_put ( process->refcnt );
	} else {
		DBGC ( process, "PROCESS %p already stopped\n", process );
	}
}

/**
 * Single-step a single process
 *
 * This executes a single step of the first process in the run queue,
 * and moves the process to the end of the run queue.
 */
void step ( void ) {
	struct process *process;

	list_for_each_entry ( process, &run_queue, list ) {
		list_del ( &process->list );
		list_add_tail ( &process->list, &run_queue );
		DBGC2 ( process, "PROCESS %p executing\n", process );
		process->step ( process );
		DBGC2 ( process, "PROCESS %p finished executing\n", process );
		break;
	}
}

/**
 * Initialise processes
 *
 */
static void init_processes ( void ) {
	struct process *process;

	for_each_table_entry ( process, PERMANENT_PROCESSES )
		process_add ( process );
}

/** Process initialiser */
struct init_fn process_init_fn __init_fn ( INIT_NORMAL ) = {
	.initialise = init_processes,
};
