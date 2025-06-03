#ifndef _IPXE_PARAMS_H
#define _IPXE_PARAMS_H

/** @file
 *
 * Request parameters
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/list.h>
#include <ipxe/refcnt.h>

/** A request parameter list */
struct parameters {
	/** Reference count */
	struct refcnt refcnt;
	/** List of all parameter lists */
	struct list_head list;
	/** Name */
	const char *name;
	/** Parameters */
	struct list_head entries;
};

/** A request parameter */
struct parameter {
	/** List of request parameters */
	struct list_head list;
	/** Key */
	const char *key;
	/** Value */
	const char *value;
	/** Flags */
	unsigned int flags;
};

/** Request parameter is a form parameter */
#define PARAMETER_FORM 0x0001

/** Request parameter is a header parameter */
#define PARAMETER_HEADER 0x0002

/**
 * Increment request parameter list reference count
 *
 * @v params		Parameter list, or NULL
 * @ret params		Parameter list as passed in
 */
static inline __attribute__ (( always_inline )) struct parameters *
params_get ( struct parameters *params ) {
	ref_get ( &params->refcnt );
	return params;
}

/**
 * Decrement request parameter list reference count
 *
 * @v params		Parameter list, or NULL
 */
static inline __attribute__ (( always_inline )) void
params_put ( struct parameters *params ) {
	ref_put ( &params->refcnt );
}

/**
 * Claim ownership of request parameter list
 *
 * @v params		Parameter list
 * @ret params		Parameter list
 */
static inline __attribute__ (( always_inline )) struct parameters *
claim_parameters ( struct parameters *params ) {

	/* Remove from list of parameter lists */
	list_del ( &params->list );

	return params;
}

/** Iterate over all request parameters in a list */
#define for_each_param( param, params )				\
	list_for_each_entry ( (param), &(params)->entries, list )

extern struct parameters * find_parameters ( const char *name );
extern struct parameters * create_parameters ( const char *name );
extern struct parameter * add_parameter ( struct parameters *params,
					  const char *key, const char *value,
					  unsigned int flags );

#endif /* _IPXE_PARAMS_H */
