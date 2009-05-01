#ifndef _GPXE_NULL_NAP_H
#define _GPXE_NULL_NAP_H

/** @file
 *
 * Null CPU sleeping
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#ifdef NAP_NULL
#define NAP_PREFIX_null
#else
#define NAP_PREFIX_null __null_
#endif

static inline __always_inline void
NAP_INLINE ( null, cpu_nap ) ( void ) {
	/* Do nothing */
}

#endif /* _GPXE_NULL_NAP_H */
