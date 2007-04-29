#ifndef _GPXE_JOB_H
#define _GPXE_JOB_H

/** @file
 *
 * Job control interfaces
 *
 */

#include <stddef.h>
#include <gpxe/interface.h>

/** Job progress */
struct job_progress {
	/** Amount of operation completed so far
	 *
	 * The units for this quantity are arbitrary.  @c completed
	 * divded by @total should give something which approximately
	 * represents the progress through the operation.  For a
	 * download operation, using byte counts would make sense.
	 */
	unsigned long completed;
	/** Total operation size
	 *
	 * See @c completed.  A zero value means "total size unknown"
	 * and is explcitly permitted; users should take this into
	 * account before calculating @c completed/total.
	 */
	unsigned long total;
};

struct job_interface;

/** Job control interface operations */
struct job_interface_operations {
	/** Job completed
	 *
	 * @v job		Job control interface
	 * @v rc		Overall job status code
	 */
	void ( * done ) ( struct job_interface *job, int rc );
	/** Abort job
	 *
	 * @v job		Job control interface
	 */
	void ( * kill ) ( struct job_interface *job );
	/** Get job progress
	 *
	 * @v job		Job control interface
	 * @v progress		Progress data to fill in
	 */
	void ( * progress ) ( struct job_interface *job,
			      struct job_progress *progress );
};

/** A job control interface */
struct job_interface {
	/** Generic object communication interface */
	struct interface intf;
	/** Operations for received messages */
	struct job_interface_operations *op;
};

extern struct job_interface null_job;
extern struct job_interface_operations null_job_ops;

extern void done ( struct job_interface *job, int rc );

extern void ignore_done ( struct job_interface *job, int rc );
extern void ignore_kill ( struct job_interface *job );
extern void ignore_progress ( struct job_interface *job,
			      struct job_progress *progress );

/**
 * Initialise a job control interface
 *
 * @v job		Job control interface
 * @v op		Job control interface operations
 * @v refcnt		Job control interface reference counting method
 */
static inline void job_init ( struct job_interface *job,
			       struct job_interface_operations *op,
			       void ( * refcnt ) ( struct interface *intf,
						   int delta ) ) {
	job->intf.dest = &null_job.intf;
	job->intf.refcnt = refcnt;
	job->op = op;
}

/**
 * Get job control interface from generic object communication interface
 *
 * @v intf		Generic object communication interface
 * @ret job		Job control interface
 */
static inline struct job_interface *
intf_to_job ( struct interface *intf ) {
	return container_of ( intf, struct job_interface, intf );
}

/**
 * Get destination job control interface
 *
 * @v job		Job control interface
 * @ret dest		Destination interface
 */
static inline struct job_interface *
job_dest ( struct job_interface *job ) {
	return intf_to_job ( job->intf.dest );
}

/**
 * Stop using a job control interface
 *
 * @v job		Job control interface
 *
 * After calling this method, no further messages will be received via
 * the interface.
 */
static inline void job_nullify ( struct job_interface *job ) {
	job->op = &null_job_ops;
};

#endif /* _GPXE_JOB_H */
