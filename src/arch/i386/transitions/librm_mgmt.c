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
#include "init.h"
#include "basemem.h"
#include "librm.h"

/*
 * This file provides functions for managing librm.
 *
 */

/* Current location of librm in base memory */
char *installed_librm = librm;
static uint32_t installed_librm_phys;

/* Whether or not we have base memory currently allocated for librm.
 * Note that we *can* have a working librm present in unallocated base
 * memory; this is the situation at startup for all real-mode
 * prefixes.
 */
static int allocated_librm = 0;

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

/*
 * Install librm to base memory
 *
 */
static inline void install_librm ( char *addr ) {
	memcpy ( addr, librm, librm_size );
	installed_librm = addr;
}

/*
 * Uninstall librm from base memory.  This copies librm back to the
 * "master" copy, so that it can be reinstalled to a new location,
 * preserving the values for rm_ss and rm_sp from the old installed
 * copy.
 *
 */
static inline void uninstall_librm ( void ) {
	memcpy ( librm, installed_librm, librm_size );
}

/*
 * On entry, record the physical location of librm.  Do this so that
 * we can update installed_librm after relocation.
 *
 * Doing this is probably more efficient than making installed_librm
 * be a physical address, because of the number of times that
 * installed_librm gets referenced in the remainder of the code.
 *
 */
static void librm_init ( void ) {
	installed_librm_phys = virt_to_phys ( installed_librm );
}

/*
 * On exit, we want to leave a copy of librm in *unallocated* base
 * memory.  It must be there so that we can exit via a 16-bit exit
 * path, but it must not be allocated because nothing will ever
 * deallocate it once we exit.
 *
 */
static void librm_exit ( void ) {
	/* Free but do not zero the base memory */
	if ( allocated_librm ) {
		free_base_memory ( installed_librm, librm_size );
		allocated_librm = 0;
	}
}

/*
 * On reset, we want to free up our old installed copy of librm, if
 * any, then allocate a new base memory block and install there.
 *
 */

static void librm_reset ( void ) {
	char *new_librm;

	/* Point installed_librm back at last known physical location */
	installed_librm = phys_to_virt ( installed_librm_phys );

	/* Uninstall old librm */
	uninstall_librm();

	/* Free allocated base memory, if applicable */
	librm_exit();

	/* Allocate space for new librm */
	new_librm = alloc_base_memory ( librm_size );
	allocated_librm = 1;

	/* Install new librm */
	install_librm ( new_librm );
}

INIT_FN ( INIT_LIBRM, librm_init, librm_reset, librm_exit );




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

#endif /* KEEP_IT_REAL */
