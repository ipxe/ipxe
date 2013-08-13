#ifndef _IPXE_PARAMS_H
#define _IPXE_PARAMS_H

/** @file
 *
 * Form parameters
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <ipxe/list.h>
#include <ipxe/refcnt.h>

/** A form parameter list */
struct parameters {
	/** List of all parameter lists */
	struct list_head list;
	/** Name */
	const char *name;
	/** Parameters */
	struct list_head entries;
};

/** A form parameter */
struct parameter {
	/** List of form parameters */
	struct list_head list;
	/** Key */
	const char *key;
	/** Value */
	const char *value;
};

/** Iterate over all form parameters in a list */
#define for_each_param( param, params )				\
	list_for_each_entry ( (param), &(params)->entries, list )

extern struct parameters * find_parameters ( const char *name );
extern struct parameters * create_parameters ( const char *name );
extern struct parameter * add_parameter ( struct parameters *params,
					  const char *key, const char *value );
extern void destroy_parameters ( struct parameters *params );
extern void claim_parameters ( struct parameters *params );

#endif /* _IPXE_PARAMS_H */
