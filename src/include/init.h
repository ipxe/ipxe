#ifndef INIT_H
#define INIT_H

/*
 * In order to avoid having objects dragged in just because main()
 * calls their initialisation function, we allow each object to
 * specify that it has a function that must be called to initialise
 * that object.  The function call_init_fns() will call all the
 * included objects' initialisation functions.
 *
 * Objects that require initialisation should include init.h and
 * register the initialisation function using INIT_FN().
 *
 * Objects may register up to three functions: init, reset and exit.
 * init gets called only once, at the point that Etherboot is
 * initialised (before the call to main()).  reset gets called between
 * each boot attempt.  exit gets called only once, just before the
 * loaded OS starts up (or just before Etherboot exits, if it exits,
 * or when the PXE NBP calls UNDI_SHUTDOWN, if it's a PXE NBP).
 *
 * The syntax is:
 *   INIT_FN ( init_order, init_function, reset_function, exit_function );
 * where init_order is an ordering taken from the list below.  Any
 * function may be left as NULL.
 */

/* An entry in the initialisation function table */

struct init_fn {
	void ( *init ) ( void );
	void ( *reset ) ( void );
	void ( *exit ) ( void );
};

/* Use double digits to avoid problems with "10" < "9" on alphabetic sort */
#define INIT_LIBRM	"00"
#define INIT_CONSOLE	"01"
#define	INIT_CPU	"02"
#define	INIT_TIMERS	"03"
#define	INIT_MEMSIZES	"04"
#define INIT_RELOCATE	"05"
#define	INIT_PCMCIA	"05"
#define	INIT_HEAP	"07"

/* Macro for creating an initialisation function table entry */
#define INIT_FN( init_order, init_func, reset_func, exit_func )		      \
	static struct init_fn init_functions				      \
	    __attribute__ ((used,__section__(".init_fns." init_order))) = {   \
		.init = init_func,					      \
		.reset = reset_func,					      \
		.exit = exit_func,					      \
	};

/* Function prototypes */

void call_init_fns ( void );
void call_reset_fns ( void );
void call_exit_fns ( void );

#endif /* INIT_H */
