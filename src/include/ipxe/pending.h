#ifndef _IPXE_PENDING_H
#define _IPXE_PENDING_H

/** @file
 *
 * Pending operations
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <ipxe/list.h>

/** A pending operation */
struct pending_operation {
	/** Pending count */
	unsigned int count;
};

/**
 * Check if an operation is pending
 *
 * @v pending		Pending operation
 * @v is_pending	Operation is pending
 */
static inline int is_pending ( struct pending_operation *pending ) {
	return ( pending->count != 0 );
}

extern void pending_get ( struct pending_operation *pending );
extern void pending_put ( struct pending_operation *pending );
extern int pending_wait ( unsigned long timeout );

#endif /* _IPXE_PENDING_H */
