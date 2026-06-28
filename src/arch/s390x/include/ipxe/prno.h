#ifndef _IPXE_PRNO_H
#define _IPXE_PRNO_H

/** @file
 *
 * Perform random number operation (PRNO)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

/** Query supported functions */
#define PRNO_FN_QUERY 0UL

/** Supported function list */
struct prno_supported {
	uint64_t mask[2];
} __attribute__ (( packed ));

/**
 * Check if function is supported
 *
 * @v supported		Supported function list
 * @v func		Function number
 * @ret is_supported	Function is supported
 */
static inline __attribute__ (( always_inline )) int
prno_is_supported ( struct prno_supported *supported, unsigned int func ) {
	uint64_t mask = supported->mask[ func / 64 ];
	uint64_t bit = ( 1UL << ( ~func % 64 ) );

	return ( !! ( mask & bit ) );
}

/** True number number generator */
#define PRNO_FN_TRNG 114UL

/** Parameter block */
union prno_parameters {
	/** Supported function list */
	struct prno_supported supported;
};

#endif /* _IPXE_PRNO_H */
