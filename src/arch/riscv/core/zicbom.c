/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Cache-block management operations (Zicbom)
 *
 * We support explicit cache management operations on I/O buffers.
 * These are guaranteed to be aligned on their own size and at least
 * as large as a (reasonable) cacheline, and therefore cannot cross a
 * cacheline boundary.
 */

#include <stdint.h>
#include <ipxe/hart.h>
#include <ipxe/xthead.h>
#include <ipxe/zicbom.h>

/** Minimum supported cacheline size
 *
 * We assume that cache management operations will ignore the least
 * significant address bits, and so we are safe to assume a cacheline
 * size that is smaller than the size actually used by the CPU.
 *
 * Cache clean and invalidate loops could be made faster by detecting
 * the actual cacheline size.
 */
#define CACHE_STRIDE 32

/** A cache management extension */
struct cache_extension {
	/**
	 * Clean data cache (i.e. write cached content back to memory)
	 *
	 * @v first		First byte
	 * @v last		Last byte
	 */
	void ( * clean ) ( const void *first, const void *last );
	/**
	 * Invalidate data cache (i.e. discard any cached content)
	 *
	 * @v first		First byte
	 * @v last		Last byte
	 */
	void ( * invalidate ) ( void *first, void *last );
};

/** Define an operation to clean the data cache */
#define CACHE_CLEAN( extension, insn )					\
	static void extension ## _clean ( const void *first,		\
					  const void *last ) {		\
									\
	__asm__ __volatile__ ( ".option arch, +" #extension "\n\t"	\
			       "\n1:\n\t"				\
			       insn "\n\t"				\
			       "addi %0, %0, %2\n\t"			\
			       "bltu %0, %1, 1b\n\t"			\
			       : "+r" ( first )				\
			       : "r" ( last ), "i" ( CACHE_STRIDE ) );	\
	}

/** Define an operation to invalidate the data cache */
#define CACHE_INVALIDATE( extension, insn )				\
	static void extension ## _invalidate ( void *first,		\
					       void *last ) {		\
									\
	__asm__ __volatile__ ( ".option arch, +" #extension "\n\t"	\
			       "\n1:\n\t"				\
			       insn "\n\t"				\
			       "addi %0, %0, %2\n\t"			\
			       "bltu %0, %1, 1b\n\t"			\
			       : "+r" ( first )				\
			       : "r" ( last ), "i" ( CACHE_STRIDE )	\
			       : "memory" );				\
	}

/** Define a cache management extension */
#define CACHE_EXTENSION( extension, clean_insn, invalidate_insn )	\
	CACHE_CLEAN ( extension, clean_insn );				\
	CACHE_INVALIDATE ( extension, invalidate_insn );		\
	static struct cache_extension extension = {			\
		.clean = extension ## _clean,				\
		.invalidate = extension ## _invalidate,			\
	};

/** The standard Zicbom extension */
CACHE_EXTENSION ( zicbom, "cbo.clean (%0)", "cbo.inval (%0)" );

/** The T-Head cache management extension */
CACHE_EXTENSION ( xtheadcmo, "th.dcache.cva %0", "th.dcache.iva %0" );

/**
 * Clean data cache (with fully coherent memory)
 *
 * @v first		First byte
 * @v last		Last byte
 */
static void cache_coherent_clean ( const void *first __unused,
				   const void *last __unused ) {
	/* Nothing to do */
}

/**
 * Invalidate data cache (with fully coherent memory)
 *
 * @v first		First byte
 * @v last		Last byte
 */
static void cache_coherent_invalidate ( void *first __unused,
					void *last __unused ) {
	/* Nothing to do */
}

/** Dummy cache management extension for fully coherent memory */
static struct cache_extension cache_coherent = {
	.clean = cache_coherent_clean,
	.invalidate = cache_coherent_invalidate,
};

static void cache_auto_detect ( void );
static void cache_auto_clean ( const void *first, const void *last );
static void cache_auto_invalidate ( void *first, void *last );

/** The autodetect cache management extension */
static struct cache_extension cache_auto = {
	.clean = cache_auto_clean,
	.invalidate = cache_auto_invalidate,
};

/** Active cache management extension */
static struct cache_extension *cache_extension = &cache_auto;

/**
 * Clean data cache (i.e. write cached content back to memory)
 *
 * @v start		Start address
 * @v len		Length
 */
void cache_clean ( const void *start, size_t len ) {
	const void *first;
	const void *last;

	/* Do nothing for zero-length buffers */
	if ( ! len )
		return;

	/* Construct address range */
	first = ( ( const void * )
		  ( ( ( intptr_t ) start ) & ~( CACHE_STRIDE - 1 ) ) );
	last = ( start + len - 1 );

	/* Clean cache lines */
	cache_extension->clean ( first, last );
}

/**
 * Invalidate data cache (i.e. discard any cached content)
 *
 * @v start		Start address
 * @v len		Length
 */
void cache_invalidate ( void *start, size_t len ) {
	void *first;
	void *last;

	/* Do nothing for zero-length buffers */
	if ( ! len )
		return;

	/* Construct address range */
	first = ( ( void * )
		  ( ( ( intptr_t ) start ) & ~( CACHE_STRIDE - 1 ) ) );
	last = ( start + len - 1 );

	/* Invalidate cache lines */
	cache_extension->invalidate ( first, last );
}

/**
 * Autodetect and clean data cache
 *
 * @v first		First byte
 * @v last		Last byte
 */
static void cache_auto_clean ( const void *first, const void *last ) {

	/* Detect cache extension */
	cache_auto_detect();

	/* Clean data cache */
	cache_extension->clean ( first, last );
}

/**
 * Autodetect and invalidate data cache
 *
 * @v first		First byte
 * @v last		Last byte
 */
static void cache_auto_invalidate ( void *first, void *last ) {

	/* Detect cache extension */
	cache_auto_detect();

	/* Clean data cache */
	cache_extension->invalidate ( first, last );
}

/**
 * Autodetect cache
 *
 */
static void cache_auto_detect ( void ) {
	int rc;

	/* Check for standard Zicbom extension */
	if ( ( rc = hart_supported ( "_zicbom" ) ) == 0 ) {
		DBGC ( &cache_extension, "CACHE detected Zicbom\n" );
		cache_extension = &zicbom;
		return;
	}

	/* Check for T-Head cache management extension */
	if ( xthead_supported ( THEAD_SXSTATUS_THEADISAEE ) ) {
		DBGC ( &cache_extension, "CACHE detected XTheadCmo\n" );
		cache_extension = &xtheadcmo;
		return;
	}

	/* Assume coherent memory if no supported extension detected */
	DBGC ( &cache_extension, "CACHE assuming coherent memory\n" );
	cache_extension = &cache_coherent;
}
