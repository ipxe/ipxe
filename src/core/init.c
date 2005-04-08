/**************************************************************************
 * call_{init,reset,exit}_fns ()
 * 
 * Call the various initialisation and exit functions.  We use a
 * function table so that we don't end up dragging in an object just
 * because we call its initialisation function.
 **************************************************************************
 */

#include "init.h"

extern struct init_fn init_fns[];
extern struct init_fn init_fns_end[];

void call_init_fns ( void ) {
	struct init_fn *init_fn;

	for ( init_fn = init_fns; init_fn < init_fns_end ; init_fn++ ) {
		if ( init_fn->init )
			init_fn->init ();
	}
}

void call_reset_fns ( void ) {
	struct init_fn *init_fn;

	for ( init_fn = init_fns; init_fn < init_fns_end ; init_fn++ ) {
		if ( init_fn->reset )
			init_fn->reset ();
	}
}

void call_exit_fns ( void ) {
	struct init_fn *init_fn;

	/* 
	 * Exit functions are called in reverse order to
	 * initialisation functions.
	 */
	for ( init_fn = init_fns_end - 1; init_fn >= init_fns ; init_fn-- ) {
		if ( init_fn->exit )
			init_fn->exit ();
	}
}
