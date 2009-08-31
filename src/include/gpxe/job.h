#ifndef _GPXE_JOB_H
#define _GPXE_JOB_H

/** @file
 *
 * Job control interfaces
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

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

extern void job_done ( struct job_interface *job, int rc );
extern void job_kill ( struct job_interface *job );
extern void job_progress ( struct job_interface *job,
			   struct job_progress *progress );

extern void ignore_job_done ( struct job_interface *job, int rc );
extern void ignore_job_kill ( struct job_interface *job );
extern void ignore_job_progress ( struct job_interface *job,
				  struct job_progress *progress );

/**
 * Initialise a job control interface
 *
 * @v job		Job control interface
 * @v op		Job control interface operations
 * @v refcnt		Containing object reference counter, or NULL
 */
static inline void job_init ( struct job_interface *job,
			      struct job_interface_operations *op,
			      struct refcnt *refcnt ) {
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
static inline __attribute__ (( always_inline )) struct job_interface *
intf_to_job ( struct interface *intf ) {
	return container_of ( intf, struct job_interface, intf );
}

/**
 * Get reference to destination job control interface
 *
 * @v job		Job control interface
 * @ret dest		Destination interface
 */
static inline __attribute__ (( always_inline )) struct job_interface *
job_get_dest ( struct job_interface *job ) {
	return intf_to_job ( intf_get ( job->intf.dest ) );
}

/**
 * Drop reference to job control interface
 *
 * @v job		Job control interface
 */
static inline __attribute__ (( always_inline )) void
job_put ( struct job_interface *job ) {
	intf_put ( &job->intf );
}

/**
 * Plug a job control interface into a new destination interface
 *
 * @v job		Job control interface
 * @v dest		New destination interface
 */
static inline void job_plug ( struct job_interface *job,
			       struct job_interface *dest ) {
	plug ( &job->intf, &dest->intf );
}

/**
 * Plug two job control interfaces together
 *
 * @v a			Job control interface A
 * @v b			Job control interface B
 */
static inline void job_plug_plug ( struct job_interface *a,
				    struct job_interface *b ) {
	plug_plug ( &a->intf, &b->intf );
}

/**
 * Unplug a job control interface
 *
 * @v job		Job control interface
 */
static inline void job_unplug ( struct job_interface *job ) {
	plug ( &job->intf, &null_job.intf );
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
