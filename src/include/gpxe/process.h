#ifndef _GPXE_PROCESS_H
#define _GPXE_PROCESS_H

/** @file
 *
 * Processes
 *
 */

#include <gpxe/list.h>

/** A process */
struct process {
	/** List of processes */
	struct list_head list;
	/**
	 * Single-step the process
	 *
	 * This method should execute a single step of the process.
	 * Returning from this method is isomorphic to yielding the
	 * CPU to another process.
	 *
	 * If the process wishes to be executed again, it must re-add
	 * itself to the run queue using schedule().
	 */
	void ( * step ) ( struct process *process );
};

extern void schedule ( struct process *process );
extern void step ( void );

#endif /* _GPXE_PROCESS_H */
