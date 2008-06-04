#ifndef _GPXE_INIT_H
#define _GPXE_INIT_H

#include <gpxe/tables.h>

/**
 * An initialisation function
 *
 * Initialisation functions are called exactly once, as part of the
 * call to initialise().
 */
struct init_fn {
	void ( * initialise ) ( void );
};

/** Declare an initialisation functon */
#define __init_fn( init_order ) \
	__table ( struct init_fn, init_fns, init_order )

/** @defgroup initfn_order Initialisation function ordering
 * @{
 */

#define INIT_EARLY	01	/**< Early initialisation */
#define INIT_SERIAL	02	/**< Serial driver initialisation */
#define	INIT_CONSOLE	03	/**< Console initialisation */
#define INIT_NORMAL	04	/**< Normal initialisation */

/** @} */

/**
 * A startup/shutdown function
 *
 * Startup and shutdown functions may be called multiple times, as
 * part of the calls to startup() and shutdown().
 */
struct startup_fn {
	void ( * startup ) ( void );
	void ( * shutdown ) ( void );
};

/** Declare a startup/shutdown function */
#define __startup_fn( startup_order ) \
	__table ( struct startup_fn, startup_fns, startup_order )

/** @defgroup startfn_order Startup/shutdown function ordering
 *
 * Shutdown functions are called in the reverse order to startup
 * functions.
 *
 * @{
 */

#define STARTUP_EARLY	01	/**< Early startup */
#define STARTUP_NORMAL	02	/**< Normal startup */

/** @} */

extern void initialise ( void );
extern void startup ( void );
extern void shutdown ( void );

#endif /* _GPXE_INIT_H */
