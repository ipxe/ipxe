/* Real-mode interface: C portions.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 */

#include "realmode.h"

/*
 * Copy data to/from base memory.
 *
 */

#ifdef KEEP_IT_REAL

void memcpy_to_real ( segoff_t dest, void *src, size_t n ) {

}

void memcpy_from_real ( void *dest, segoff_t src, size_t n ) {

}

#endif /* KEEP_IT_REAL */
