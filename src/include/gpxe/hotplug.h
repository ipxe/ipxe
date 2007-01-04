#ifndef _GPXE_HOTPLUG_H
#define _GPXE_HOTPLUG_H

/** @file
 *
 * Hotplug support
 *
 */

#include <gpxe/list.h>

/**
 * A persistent reference to another data structure
 *
 * This data structure should be embedded within any data structure
 * (the referrer) which holds a persistent reference to a separate,
 * volatile data structure (the referee).
 */
struct reference {
	/** List of persistent references */
	struct list_head list;
	/** Forget persistent reference
	 *
	 * @v ref		Persistent reference
	 *
	 * This method is called immediately before the referred-to
	 * data structure is destroyed.  The reference holder must
	 * forget all references to the referee before returning from
	 * this method.
	 *
	 * This method must also call ref_del() to remove the
	 * reference.
	 */
	void ( * forget ) ( struct reference *ref );
};

/**
 * Add persistent reference
 *
 * @v ref		Persistent reference
 * @v list		List of persistent references
 */
static inline void ref_add ( struct reference *ref, struct list_head *list ) {
	list_add ( &ref->list, list );
}

/**
 * Remove persistent reference
 *
 * @v ref		Persistent reference
 */
static inline void ref_del ( struct reference *ref ) {
	list_del ( &ref->list );
}

extern void forget_references ( struct list_head *list );

#endif /* _GPXE_HOTPLUG_H */
