#ifndef _GPXE_ASYNC_H
#define _GPXE_ASYNC_H

/** @file
 *
 * Asynchronous operations
 *
 */

#include <gpxe/list.h>

struct async;

/** An asynchronous operation ID
 *
 * Only positive identifiers are valid; negative values are used to
 * indicate errors.
 */
typedef long aid_t;

/** Signals that can be delivered to asynchronous operations */
enum signal {
	/** A child asynchronous operation has completed
	 *
	 * The parent should call async_wait() to reap the completed
	 * child.  async_wait() will return the exit status and
	 * operation identifier of the child.
	 *
	 * The handler for this signal can be set to @c NULL; if it
	 * is, then the children will accumulate as zombies until
	 * async_wait() is called.
	 *
	 * The handler for this signal can also be set to @c SIG_IGN;
	 * if it is, then the children will automatically be reaped.
	 * Note that if you use @c SIG_IGN then you will not be able
	 * to retrieve the return status of the children; the call to
	 * async_wait() will simply return -ECHILD.
	 */
	SIGCHLD = 0,
	/** Cancel asynchronous operation
	 *
	 * This signal should trigger the asynchronous operation to
	 * cancel itself (including killing all its own children, if
	 * any), and then call async_done().  The asynchronous
	 * operation is allowed to not complete immediately.
	 *
	 * The handler for this signal can be set to @c NULL; if it
	 * is, then attempts to cancel the asynchronous operation will
	 * fail and the operation will complete normally.  Anything
	 * waiting for the operation to cancel will block.
	 */
	SIGKILL,
	/** Update progress of asynchronous operation
	 *
	 * This signal should cause the asynchronous operation to
	 * immediately update the @c completed and @c total fields.
	 *
	 * The handler for this signal can be set to @c NULL; if it
	 * is, then the asynchronous operation is expected to keep its
	 * @c completed and @c total fields up to date at all times.
	 */
	SIGUPDATE,
	SIGMAX
};

/**
 * A signal handler
 *
 * @v async		Asynchronous operation
 * @v signal		Signal received
 */
typedef void ( * signal_handler_t ) ( struct async *async,
				      enum signal signal );

/** Asynchronous operation operations */
struct async_operations {
	/** Reap asynchronous operation
	 *
	 * @v async		Asynchronous operation
	 *
	 * Release all resources associated with the asynchronous
	 * operation.  This will be called only after the asynchronous
	 * operation itself calls async_done(), so the only remaining
	 * resources will probably be the memory used by the struct
	 * async itself.
	 *
	 * This method can be set to @c NULL; if it is, then no
	 * resources will be freed.  This may be suitable for
	 * asynchronous operations that consume no dynamically
	 * allocated memory.
	 */
	void ( * reap ) ( struct async *async );
	/** Handle signals */
	signal_handler_t signal[SIGMAX];
};

/** An asynchronous operation */
struct async {
	/** Other asynchronous operations with the same parent */
	struct list_head siblings;
	/** Child asynchronous operations */
	struct list_head children;
	/** Parent asynchronous operation
	 *
	 * This field is optional; if left to NULL then the owner must
	 * never call async_done().
	 */
	struct async *parent;
	/** Asynchronous operation ID */
	aid_t aid;
	/** Final return status code */
	int rc;

	/** Amount of operation completed so far
	 *
	 * The units for this quantity are arbitrary.  @c completed
	 * divded by @total should give something which approximately
	 * represents the progress through the operation.  For a
	 * download operation, using byte counts would make sense.
	 *
	 * This progress indicator should also incorporate the status
	 * of any child asynchronous operations.
	 */
	unsigned long completed;
	/** Total operation size
	 *
	 * See @c completed.  A zero value means "total size unknown"
	 * and is explcitly permitted; users should take this into
	 * account before calculating @c completed/total.
	 */
	unsigned long total;

	struct async_operations *aop;
};

extern struct async_operations default_async_operations;
extern struct async_operations orphan_async_operations;

extern aid_t async_init ( struct async *async, struct async_operations *aop,
			  struct async *parent );
extern void async_ignore_signal ( struct async *async, enum signal signal );
extern void async_signal ( struct async *async, enum signal signal );
extern void async_signal_children ( struct async *async, enum signal signal );
extern void async_done ( struct async *async, int rc );
extern aid_t async_wait ( struct async *async, int *rc, int block );

/** Default signal handler */
#define SIG_DFL NULL

/** Ignore signal */
#define SIG_IGN async_ignore_signal

/**
 * Initialise orphan asynchronous operation
 *
 * @v async		Asynchronous operation
 * @ret aid		Asynchronous operation ID
 *
 * An orphan asynchronous operation can act as a context for child
 * operations.  However, you must not call async_done() on such an
 * operation, since this would attempt to send a signal to its
 * (non-existent) parent.  Instead, simply free the structure (after
 * calling async_wait() to ensure that any child operations have
 * completed).
 */
static inline aid_t async_init_orphan ( struct async *async ) {
	return async_init ( async, &orphan_async_operations, NULL );
}

/**
 * Execute and block on an asynchronous operation
 *
 * @v async_temp	Temporary asynchronous operation structure to use
 * @v START		Code used to start the asynchronous operation
 * @ret rc		Return status code
 *
 * This is a notational shorthand for writing
 *
 *     	async_init_orphan ( &async_temp );
 *	if ( ( rc = START ) == 0 )
 *		async_wait ( &async_temp );
 *      if ( rc != 0 ) {
 *         ...handle failure...
 *      }
 *
 * and allows you instead to write
 *
 *      if ( ( rc = async_block ( &async_temp, START ) ) != 0 ) {
 *         ...handle failure...
 *      }
 *
 * The argument START is a code snippet; it should initiate an
 * asynchronous operation as a child of @c async_temp and return an
 * error status code if it failed to do so (e.g. due to malloc()
 * failure).
 */
#define async_block( async_temp, START ) ( {			\
		int rc;						\
								\
	 	async_init_orphan ( async_temp );		\
		if ( ( rc = START ) == 0 )			\
			async_wait ( async_temp, &rc, 1 );	\
		rc;						\
	} )

#endif /* _GPXE_ASYNC_H */
