/*
 * Segment:offset types and macros
 *
 * Initially written by Michael Brown (mcb30).
 */

#ifndef SEGOFF_H
#define SEGOFF_H

#include <stdint.h>
#include <osdep.h>
#include <io.h>

/* Segment:offset structure.  Note that the order within the structure
 * is offset:segment.
 */
typedef struct {
	uint16_t offset;
	uint16_t segment;
} segoff_t;

/* Macros for converting from virtual to segment:offset addresses,
 * when we don't actually care which of the many isomorphic results we
 * get.
 */
#ifdef DEBUG_SEGMENT
uint16_t SEGMENT ( const void * const ptr ) {
	uint32_t phys = virt_to_phys ( ptr );
	if ( phys > 0xfffff ) {
		printf ( "FATAL ERROR: segment address out of range\n" );
	}
	return phys >> 4;
}
#else
#define SEGMENT(x) ( virt_to_phys ( x ) >> 4 )
#endif
#define OFFSET(x) ( virt_to_phys ( x ) & 0xf )
#define SEGOFF(x) { OFFSET(x), SEGMENT(x) }
#define VIRTUAL(x,y) ( phys_to_virt ( ( ( x ) << 4 ) + ( y ) ) )

#endif /* SEGOFF_H */
