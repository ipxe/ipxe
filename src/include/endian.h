#ifndef _ENDIAN_H
#define _ENDIAN_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** Constant representing little-endian byte order
 *
 * Little-endian systems should define BYTE_ORDER as LITTLE_ENDIAN.
 * This constant is intended to be used only at compile time.
 */
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 0x44332211UL
#endif

/** Constant representing big-endian byte order
 *
 * Big-endian systems should define BYTE_ORDER as BIG_ENDIAN.
 * This constant is intended to be used only at compile time.
 */
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN 0x11223344UL
#endif

#include "bits/endian.h"

#endif /* _ENDIAN_H */
