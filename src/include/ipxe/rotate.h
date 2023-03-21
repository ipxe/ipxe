#ifndef _IPXE_ROTATE_H
#define _IPXE_ROTATE_H

/** @file
 *
 * Bit operations
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

#define ROLx( data, rotation )						\
	( ( (data) << (rotation) ) |					\
	  ( (data) >> ( ( 8 * sizeof (data) ) - (rotation) ) ) );

#define RORx( data, rotation )						\
	( ( (data) >> (rotation) ) |					\
	  ( (data) << ( ( 8 * sizeof (data) ) - (rotation) ) ) );

static inline __attribute__ (( always_inline )) uint8_t
rol8 ( uint8_t data, unsigned int rotation ) {
	return ROLx ( data, rotation );
}

static inline __attribute__ (( always_inline )) uint8_t
ror8 ( uint8_t data, unsigned int rotation ) {
	return RORx ( data, rotation );
}

static inline __attribute__ (( always_inline )) uint16_t
rol16 ( uint16_t data, unsigned int rotation ) {
	return ROLx ( data, rotation );
}

static inline __attribute__ (( always_inline )) uint16_t
ror16 ( uint16_t data, unsigned int rotation ) {
	return RORx ( data, rotation );
}

static inline __attribute__ (( always_inline )) uint32_t
rol32 ( uint32_t data, unsigned int rotation ) {
	return ROLx ( data, rotation );
}

static inline __attribute__ (( always_inline )) uint32_t
ror32 ( uint32_t data, unsigned int rotation ) {
	return RORx ( data, rotation );
}

static inline __attribute__ (( always_inline )) uint64_t
rol64 ( uint64_t data, unsigned int rotation ) {
	return ROLx ( data, rotation );
}

static inline __attribute__ (( always_inline )) uint64_t
ror64 ( uint64_t data, unsigned int rotation ) {
	return RORx ( data, rotation );
}

static inline __attribute__ (( always_inline )) unsigned long
roll ( unsigned long data, unsigned int rotation ) {
	return ROLx ( data, rotation );
}

static inline __attribute__ (( always_inline )) unsigned long
rorl ( unsigned long data, unsigned int rotation ) {
	return RORx ( data, rotation );
}

#endif /* _IPXE_ROTATE_H */
