#ifndef _IPXE_QUIESCE_H
#define _IPXE_QUIESCE_H

/** @file
 *
 * Quiesce system
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/tables.h>

/** A quiescer */
struct quiescer {
	/** Quiesce system */
	void ( * quiesce ) ( void );
	/** Unquiesce system */
	void ( * unquiesce ) ( void );
};

/** Quiescer table */
#define QUIESCERS __table ( struct quiescer, "quiescers" )

/** Declare a quiescer */
#define __quiescer __table_entry ( QUIESCERS, 01 )

extern void quiesce ( void );
extern void unquiesce ( void );

#endif /* _IPXE_QUIESCE_H */
