/*
 * librm: a library for interfacing to real-mode code
 *
 * Michael Brown <mbrown@fensystems.co.uk>
 *
 */

#ifdef KEEP_IT_REAL
/* Build a null object under -DKEEP_IT_REAL */
#else

#include <stdint.h>
#include <librm.h>

/*
 * This file provides functions for managing librm.
 *
 */

/*
 * Allocate space on the real-mode stack and copy data there.
 *
 */
uint16_t copy_to_rm_stack ( void *data, size_t size ) {
#ifdef DEBUG_LIBRM
	if ( rm_sp <= size ) {
		printf ( "librm: out of space in RM stack\n" );
		lockup();
	}
#endif
	rm_sp -= size;
	copy_to_real ( rm_ss, rm_sp, data, size );
	return rm_sp;
};

/*
 * Deallocate space on the real-mode stack, optionally copying back
 * data.
 *
 */
void remove_from_rm_stack ( void *data, size_t size ) {
	if ( data ) {
		copy_from_real ( data, rm_ss, rm_sp, size );
	}
	rm_sp += size;
};

#endif /* KEEP_IT_REAL */
