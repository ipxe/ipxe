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

#include <string.h>
#include <errno.h>
#include <assert.h>
#include <gpxe/process.h>
#include <gpxe/async.h>

/** @file
 *
 * Asynchronous operations
 *
 */

/**
 * Name signal
 *
 * @v signal		Signal number
 * @ret name		Name of signal
 */
static inline __attribute__ (( always_inline )) const char *
signal_name ( enum signal signal ) {
	switch ( signal ) {
	case SIGCHLD:		return "SIGCHLD";
	case SIGKILL:		return "SIGKILL";
	case SIGUPDATE:		return "SIGUPDATE";
	default:		return "SIG<UNKNOWN>";
	}
}

/**
 * Initialise an asynchronous operation
 *
 * @v async		Asynchronous operation
 * @v aop		Asynchronous operation operations to use
 * @v parent		Parent asynchronous operation, or NULL
 * @ret aid		Asynchronous operation ID
 *
 * It is valid to create an asynchronous operation with no parent
 * operation; see async_init_orphan().
 */
aid_t async_init ( struct async *async, struct async_operations *aop,
		   struct async *parent ) {
	static aid_t aid = 1;

	/* Assign identifier.  Negative IDs are used to indicate
	 * errors, so avoid assigning them.
	 */
	++aid;
	aid &= ( ( ~( ( aid_t ) 0 ) ) >> 1 );

	DBGC ( async, "ASYNC %p (type %p) initialising as", async, aop );
	if ( parent ) {
		DBGC ( async, " child of ASYNC %p", parent );
	} else {
		DBGC ( async, " orphan" );
	}
	DBGC ( async, " with ID %ld\n", aid );

	assert ( async != NULL );
	assert ( aop != NULL );

	/* Add to hierarchy */
	if ( parent ) {
		async->parent = parent;
		list_add ( &async->siblings, &parent->children );
	}
	INIT_LIST_HEAD ( &async->children );

	/* Initialise fields */
	async->rc = -EINPROGRESS;
	async->completed = 0;
	async->total = 0;
	async->aop = aop;
	async->aid = aid;

	return async->aid;
}

/**
 * Uninitialise an asynchronous operation
 *
 * @v async		Asynchronous operation
 *
 * Abandon an asynchronous operation without signalling the parent.
 * You may do this only during the period between calling async_init()
 * and returning to the parent for the first time.  It is designed to
 * simplify the error paths of asynchronous operations that themselves
 * spawn further asynchronous operations.
 *
 * An example may help:
 *
 *     int start_something ( ..., struct async *parent ) {
 *         struct my_data_structure *myself;
 *
 *         ... allocate memory for myself ...
 *
 *         async_init ( &myself->async, &my_async_operations, parent );
 *         if ( ( rc = start_child_operation ( ..., &myself->async ) ) != 0 ) {
 *             async_uninit ( &myself->async );
 *             return rc;
 *         }
 *
 *         return 0;
 *     }
 *
 * It is valid to call async_uninit() on an asynchronous operation
 * that has not yet been initialised (i.e. a zeroed-out @c struct @c
 * async).
 */
void async_uninit ( struct async *async ) {

	assert ( async != NULL );

	if ( async->parent ) {
		assert ( list_empty ( &async->children ) );

		DBGC ( async, "ASYNC %p uninitialising\n", async );
		list_del ( &async->siblings );
	}
}

/**
 * SIGCHLD 'ignore' handler
 *
 * @v async		Asynchronous operation
 * @v signal		Signal received
 */
static void async_ignore_sigchld ( struct async *async, enum signal signal ) {
	aid_t waited_aid;

	assert ( async != NULL );
	assert ( signal == SIGCHLD );

	/* Reap the child */
	waited_aid = async_wait ( async, NULL, 0 );
	assert ( waited_aid >= 0 );
}

/**
 * SIGUPDATE 'ignore' handler
 *
 * @v async		Asynchronous operation
 * @v signal		Signal received
 */
static void async_ignore_sigupdate ( struct async *async,
				     enum signal signal ) {
	struct async *child;

	assert ( async != NULL );
	assert ( signal == SIGUPDATE );

	async_signal_children ( async, signal );
	async->completed = 0;
	async->total = 0;
	list_for_each_entry ( child, &async->children, siblings ) {
		async->completed += child->completed;
		async->total += child->total;
	}
}

/**
 * 'Ignore' signal handler
 *
 * @v async		Asynchronous operation
 * @v signal		Signal received
 */
void async_ignore_signal ( struct async *async, enum signal signal ) {

	DBGC ( async, "ASYNC %p using ignore handler for %s\n",
	       async, signal_name ( signal ) );

	assert ( async != NULL );

	switch ( signal ) {
	case SIGCHLD:
		async_ignore_sigchld ( async, signal );
		break;
	case SIGUPDATE:
		async_ignore_sigupdate ( async, signal );
		break;
	case SIGKILL:
	default:
		/* Nothing to do */
		break;
	}
}

/**
 * Default signal handler
 *
 * @v async		Asynchronous operation
 * @v signal		Signal received
 */
static void async_default_signal ( struct async *async, enum signal signal ) {

	DBGC ( async, "ASYNC %p using default handler for %s\n",
	       async, signal_name ( signal ) );

	assert ( async != NULL );

	switch ( signal ) {
	case SIGCHLD:
	case SIGKILL:
	case SIGUPDATE:
	default:
		/* Nothing to do */
		break;
	}
}

/**
 * Send signal to asynchronous operation
 *
 * @v async		Asynchronous operation
 * @v signal		Signal to send
 */
void async_signal ( struct async *async, enum signal signal ) {
	signal_handler_t handler;

	DBGC ( async, "ASYNC %p receiving %s\n",
	       async, signal_name ( signal ) );

	assert ( async != NULL );
	assert ( async->aop != NULL );
	assert ( signal < SIGMAX );

	handler = async->aop->signal[signal];
	if ( handler ) {
		/* Use the asynchronous operation's signal handler */
		handler ( async, signal );
	} else {
		/* Use the default handler */
		async_default_signal ( async, signal );
	}
}

/**
 * Send signal to all child asynchronous operations
 *
 * @v async		Asynchronous operation
 * @v signal		Signal to send
 */
void async_signal_children ( struct async *async, enum signal signal ) {
	struct async *child;
	struct async *tmp;

	assert ( async != NULL );

	list_for_each_entry_safe ( child, tmp, &async->children, siblings ) {
		async_signal ( child, signal );
	}
}

/**
 * Reap default handler
 *
 * @v async		Asynchronous operation
 */
static void async_reap_default ( struct async *async ) {

	DBGC ( async, "ASYNC %p ignoring REAP\n", async );

	assert ( async != NULL );

	/* Nothing to do */
}

/**
 * Reap asynchronous operation
 *
 * @v async		Asynchronous operation
 *
 * Note that the asynchronous operation should have been freed by
 * calling this function; you may not dereference @c async after this
 * call.
 */
static void async_reap ( struct async *async ) {

	DBGC ( async, "ASYNC %p being reaped, exit status %d (%s)\n",
	       async, async->rc, strerror ( async->rc ) );

	assert ( async != NULL );
	assert ( async->aop != NULL );
	assert ( list_empty ( &async->children ) );

	/* Unlink from hierarchy */
	if ( async->parent )
		list_del ( &async->siblings );
	async->parent = NULL;

	/* Release all resources */
	if ( async->aop->reap ) {
		async->aop->reap ( async );
	} else {
		async_reap_default ( async );
	}
}

/**
 * Mark asynchronous operation as complete
 *
 * @v async		Asynchronous operation
 * @v rc		Return status code
 *
 * An asynchronous operation should call this once it has completed.
 * After calling async_done(), it must be prepared to be reaped by
 * having its reap() method called.
 */
void async_done ( struct async *async, int rc ) {
	struct async *child;
	struct async *tmp;

	DBGC ( async, "ASYNC %p completing with status %d (%s)\n",
	       async, rc, strerror ( rc ) );

	assert ( async != NULL );
	assert ( async->parent != NULL );
	assert ( rc != -EINPROGRESS );

	/* Store return status code */
	async->rc = rc;

	/* Disown all of our children */
	list_for_each_entry_safe ( child, tmp, &async->children, siblings ) {
		DBGC ( async, "ASYNC %p disowning child ASYNC %p\n",
		       async, child );
		list_del ( &child->siblings );
		child->parent = NULL;
	}

	/* Send SIGCHLD to parent.  If we don't have a parent then we
	 * have to take care of our own funeral arrangements.
	 */
	if ( async->parent ) {
		async_signal ( async->parent, SIGCHLD );
	} else {
		async_reap ( async );
	}
}

/**
 * Wait for any child asynchronous operation to complete
 * 
 * @v child		Child asynchronous operation
 * @v rc		Child exit status to fill in, or NULL
 * @v block		Block waiting for child operation to complete
 * @ret aid		Asynchronous operation ID, or -1 on error
 */
aid_t async_wait ( struct async *async, int *rc, int block ) {
	struct async *child;
	aid_t child_aid;
	int dummy_rc;

	DBGC ( async, "ASYNC %p performing %sblocking wait%s\n", async,
	       ( block ? "" : "non-" ), ( rc ? "" : " (ignoring status)" ) );

	assert ( async != NULL );

	/* Avoid multiple tests for "if ( rc )" */
	if ( ! rc )
		rc = &dummy_rc;

	while ( 1 ) {

		/* Return immediately if we have no children */
		if ( list_empty ( &async->children ) ) {
			DBGC ( async, "ASYNC %p has no more children\n",
			       async );
			*rc = -ECHILD;
			return -1;
		}

		/* Look for a completed child */
		list_for_each_entry ( child, &async->children, siblings ) {
			if ( child->rc == -EINPROGRESS )
				continue;

			/* Found a completed child */
			*rc = child->rc;
			child_aid = child->aid;

			DBGC ( async, "ASYNC %p reaping child ASYNC %p "
			       "(ID %ld)\n", async, child, child_aid );

			/* Reap the child and return */
			async_reap ( child );
			return child_aid;
		}

		/* Return immediately if non-blocking */
		if ( ! block ) {
			*rc = -EINPROGRESS;
			return -1;
		}

		/* Allow processes to run */
		step();
	}
}

/**
 * Wait for any child asynchronous operation to complete, with progress bar
 * 
 * @v child		Child asynchronous operation
 * @v rc		Child exit status to fill in, or NULL
 * @ret aid		Asynchronous operation ID, or -1 on error
 */
aid_t async_wait_progress ( struct async *async, int *rc ) {
	struct async *child;
	long last_progress = -1;
	long progress;
	aid_t child_aid;

	do {
		step();
		async_signal ( async, SIGUPDATE );
		if ( async->total ) {
			progress = ( async->completed / (async->total / 100) );
			if ( progress != last_progress )
				printf ( "\rProgress: %d%%", progress );
			last_progress = progress;
		}
		child_aid = async_wait ( async, rc, 0 );
	} while ( *rc == -EINPROGRESS );

	printf ( "\n" );
	return child_aid;
}

/**
 * Default asynchronous operations
 *
 * The default is to ignore SIGCHLD (i.e. to automatically reap
 * children) and to use the default handler (i.e. do nothing) for all
 * other signals.
 */
struct async_operations default_async_operations = {
	.signal = {
		[SIGCHLD]	= SIG_IGN,
		[SIGUPDATE]	= SIG_IGN,
	},
};

/**
 * Default asynchronous operations for orphan asynchronous operations
 *
 * The default for orphan asynchronous operations is to do nothing for
 * SIGCHLD (i.e. to not automatically reap children), on the
 * assumption that you're probably creating the orphan solely in order
 * to async_wait() on it.
 */
struct async_operations orphan_async_operations = {
	.signal = {
		[SIGCHLD]	= SIG_DFL,
		[SIGUPDATE]	= SIG_IGN,
	},
};
