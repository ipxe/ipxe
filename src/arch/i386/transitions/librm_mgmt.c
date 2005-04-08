/*
 * librm: a library for interfacing to real-mode code
 *
 * Michael Brown <mbrown@fensystems.co.uk>
 *
 */

#ifdef KEEP_IT_REAL
/* Build a null object under -DKEEP_IT_REAL */
#else

#include "stdint.h"
#include "stddef.h"
#include "string.h"
#include "librm.h"

/*
 * This file provides functions for managing librm.
 *
 */

/* Current location of librm in base memory */
char *installed_librm = librm;

/*
 * Install librm to base memory
 *
 */
void install_librm ( void *addr ) {
	memcpy ( addr, librm, librm_size );
	installed_librm = addr;
}

/*
 * Increment lock count of librm
 *
 */
void lock_librm ( void ) {
	inst_librm_ref_count++;
}

/*
 * Decrement lock count of librm
 *
 */
void unlock_librm ( void ) {
#ifdef DEBUG_LIBRM
	if ( inst_librm_ref_count == 0 ) {
		printf ( "librm: ref count gone negative\n" );
		lockup();
	}
#endif
	inst_librm_ref_count--;
}

/*
 * Allocate space on the real-mode stack and copy data there.
 *
 */
uint16_t copy_to_rm_stack ( void *data, size_t size ) {
#ifdef DEBUG_LIBRM
	if ( inst_rm_stack.offset <= size ) {
		printf ( "librm: out of space in RM stack\n" );
		lockup();
	}
#endif
	inst_rm_stack.offset -= size;
	copy_to_real ( inst_rm_stack.segment, inst_rm_stack.offset,
		       data, size );
	return inst_rm_stack.offset;
};

/*
 * Deallocate space on the real-mode stack, optionally copying back
 * data.
 *
 */
void remove_from_rm_stack ( void *data, size_t size ) {
	if ( data ) {
		copy_from_real ( data,
				 inst_rm_stack.segment, inst_rm_stack.offset,
				 size );
	}
	inst_rm_stack.offset += size;
};

#endif /* KEEP_IT_REAL */
