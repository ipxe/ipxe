#ifndef _IPXE_MEMMAP_H
#define _IPXE_MEMMAP_H

/** @file
 *
 * System memory map
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stddef.h>
#include <stdint.h>
#include <ipxe/api.h>
#include <ipxe/tables.h>
#include <config/ioapi.h>

/**
 * Calculate static inline memory map API function name
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @ret _subsys_func	Subsystem API function
 */
#define MEMMAP_INLINE( _subsys, _api_func ) \
	SINGLE_API_INLINE ( MEMMAP_PREFIX_ ## _subsys, _api_func )

/**
 * Provide a memory map API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @v _func		Implementing function
 */
#define PROVIDE_MEMMAP( _subsys, _api_func, _func ) \
	PROVIDE_SINGLE_API ( MEMMAP_PREFIX_ ## _subsys, _api_func, _func )

/**
 * Provide a static inline memory map API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 */
#define PROVIDE_MEMMAP_INLINE( _subsys, _api_func ) \
	PROVIDE_SINGLE_API_INLINE ( MEMMAP_PREFIX_ ## _subsys, _api_func )

/** A memory region descriptor */
struct memmap_region {
	/** Minimum address in region */
	uint64_t min;
	/** Maximum address in region */
	uint64_t max;
	/** Region flags */
	unsigned int flags;
	/** Region name (for debug messages) */
	const char *name;
};

#define MEMMAP_FL_MEMORY	0x0001	/**< Contains memory */
#define MEMMAP_FL_RESERVED	0x0002	/**< Is reserved */
#define MEMMAP_FL_USED		0x0004	/**< Is in use by iPXE */
#define MEMMAP_FL_INACCESSIBLE	0x0008	/**< Outside of addressable range */

/**
 * Initialise memory region descriptor
 *
 * @v min		Minimum address
 * @v region		Region descriptor to fill in
 */
static inline __attribute__ (( always_inline )) void
memmap_init ( uint64_t min, struct memmap_region *region ) {

	region->min = min;
	region->max = ~( ( uint64_t ) 0 );
	region->flags = 0;
	region->name = NULL;
}

/**
 * Check if memory region is usable
 *
 * @v region		Region descriptor
 * @ret is_usable	Memory region is usable
 */
static inline __attribute__ (( always_inline )) int
memmap_is_usable ( const struct memmap_region *region ) {

	return ( region->flags == MEMMAP_FL_MEMORY );
}

/**
 * Get remaining size of memory region (from the described address upwards)
 *
 * @v region		Region descriptor
 * @ret size		Size of memory region
 */
static inline __attribute__ (( always_inline )) uint64_t
memmap_size ( const struct memmap_region *region ) {

	/* Calculate size, assuming overflow is known to be impossible */
	return ( region->max - region->min + 1 );
}

/** An in-use memory region */
struct used_region {
	/** Region name */
	const char *name;
	/** Start address */
	physaddr_t start;
	/** Length of region */
	size_t size;
};

/** In-use memory region table */
#define USED_REGIONS __table ( struct used_region, "used_regions" )

/** Declare an in-use memory region */
#define __used_region __table_entry ( USED_REGIONS, 01 )

/* Include all architecture-independent ACPI API headers */
#include <ipxe/null_memmap.h>
#include <ipxe/fdtmem.h>

/* Include all architecture-dependent ACPI API headers */
#include <bits/memmap.h>

/**
 * Describe memory region from system memory map
 *
 * @v min		Minimum address
 * @v hide		Hide in-use regions from the memory map
 * @v region		Region descriptor to fill in
 */
void memmap_describe ( uint64_t min, int hide, struct memmap_region *region );

/**
 * Synchronise in-use regions with the externally visible system memory map
 *
 * In environments such as x86 BIOS, we need to patch the global
 * system memory map to hide our in-use regions, since there is no
 * other way to communicate this information to external code.
 */
void memmap_sync ( void );

/**
 * Update an in-use memory region
 *
 * @v used		In-use memory region
 * @v start		Start address
 * @v size		Length of region
 */
static inline __attribute__ (( always_inline )) void
memmap_use ( struct used_region *used, physaddr_t start, size_t size ) {

	/* Record region */
	used->start = start;
	used->size = size;

	/* Synchronise externally visible memory map */
	memmap_sync();
}

/**
 * Iterate over memory regions from a given starting address
 *
 * @v region		Region descriptor
 * @v start		Starting address
 * @v hide		Hide in-use regions from the memory map
 */
#define for_each_memmap_from( region, start, hide )			\
	for ( (region)->min = (start), (region)->max = 0 ;		\
	      ( ( ( (region)->max + 1 ) != 0 ) &&			\
		( memmap_describe ( (region)->min, (hide),		\
				    (region) ), 1 ) ) ;			\
	      (region)->min = ( (region)->max + 1 ) )

/**
 * Iterate over memory regions
 *
 * @v region		Region descriptor
 * @v hide		Hide in-use regions from the memory map
 */
#define for_each_memmap( region, hide )					\
	for_each_memmap_from ( (region), 0, (hide) )

#define DBG_MEMMAP_IF( level, region ) do {				\
	const char *name = (region)->name;				\
	unsigned int flags = (region)->flags;				\
									\
	DBG_IF ( level, "MEMMAP (%s%s%s%s) [%#08llx,%#08llx]%s%s\n",	\
		 ( ( flags & MEMMAP_FL_MEMORY ) ? "M" : "-" ),		\
		 ( ( flags & MEMMAP_FL_RESERVED ) ? "R" : "-" ),	\
		 ( ( flags & MEMMAP_FL_USED ) ? "U" : "-" ),		\
		 ( ( flags & MEMMAP_FL_INACCESSIBLE ) ? "X" : "-" ),	\
		 ( ( unsigned long long ) (region)->min ),		\
		 ( ( unsigned long long ) (region)->max ),		\
		 ( name ? " " : "" ), ( name ? name : "" ) );		\
	} while ( 0 )

#define DBGC_MEMMAP_IF( level, id, ... ) do {				\
		DBG_AC_IF ( level, id );				\
		DBG_MEMMAP_IF ( level, __VA_ARGS__ );			\
		DBG_DC_IF ( level );					\
	} while ( 0 )

#define DBGC_MEMMAP( ... )	DBGC_MEMMAP_IF ( LOG, ##__VA_ARGS__ )
#define DBGC2_MEMMAP( ... )	DBGC_MEMMAP_IF ( EXTRA, ##__VA_ARGS__ )
#define DBGCP_MEMMAP( ... )	DBGC_MEMMAP_IF ( PROFILE, ##__VA_ARGS__ )

/**
 * Dump system memory map (for debugging)
 *
 * @v hide		Hide in-use regions from the memory map
 */
static inline void memmap_dump_all ( int hide ) {
	struct memmap_region region;

	/* Do nothing unless debugging is enabled */
	if ( ! DBG_LOG )
		return;

	/* Describe all memory regions */
	DBGC ( &memmap_describe, "MEMMAP with in-use regions %s:\n",
	       ( hide ? "hidden" : "ignored" ) );
	for_each_memmap ( &region, hide )
		DBGC_MEMMAP ( &memmap_describe, &region );
}

extern void memmap_update ( struct memmap_region *region, uint64_t start,
			    uint64_t size, unsigned int flags,
			    const char *name );
extern void memmap_update_used ( struct memmap_region *region );
extern size_t memmap_largest ( physaddr_t *start );

#endif /* _IPXE_MEMMAP_H */
