#ifndef _GPXE_BITOPS_H
#define _GPXE_BITOPS_H

/** @file
 *
 * Bit operations
 */

#include <stdint.h>

static inline uint32_t rol32 ( uint32_t data, unsigned int rotation ) {
        return ( ( data << rotation ) | ( data >> ( 32 - rotation ) ) );
}

static inline uint32_t ror32 ( uint32_t data, unsigned int rotation ) {
        return ( ( data >> rotation ) | ( data << ( 32 - rotation ) ) );
}

#endif /* _GPXE_BITOPS_H */
