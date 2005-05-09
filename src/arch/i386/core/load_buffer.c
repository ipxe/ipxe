#include "limits.h"
#include "io.h"
#include "memsizes.h"
#include "init.h"
#include "buffer.h"

/*
 * Initialise a buffer in an unused portion of memory, to be used for
 * loading an image
 *
 */

/* Under KEEP_IT_REAL, always use 07c0:0000 as the buffer.  Otherwise,
 * use a reset_fn that finds the largest available block of RAM.
 */
struct buffer load_buffer = {
	.start = 0x7c00,
	.end = 0xa0000,
};

#ifndef KEEP_IT_REAL

extern char _text[];

static void init_load_buffer ( void ) {
	unsigned int i;
	unsigned long size = 0;

	load_buffer.start = 0;
	load_buffer.end = 0;

	/* Find the largest usable segment in memory */
	for ( i = 0 ; i < meminfo.map_count ; i++ ) {
		unsigned long r_start, r_end;

		if ( meminfo.map[i].type != E820_RAM )
			continue;

		if ( meminfo.map[i].addr + meminfo.map[i].size > ULONG_MAX )
			continue;

		r_start = meminfo.map[i].addr;
		r_end = meminfo.map[i].size;

		/* Avoid overlap with Etherboot.  Etherboot will be
		 * located towards the top of a segment, so we need
		 * only consider one-sided truncation.
		 */
		if ( ( r_start <= virt_to_phys ( _text ) ) &&
		     ( virt_to_phys ( _text ) < r_end ) ) {
			r_end = virt_to_phys ( _text );
		}

		if ( r_end - r_start > size ) {
			size = r_end - r_start;
			load_buffer.start = r_start;
			load_buffer.end = r_end;
		}
	}
}

INIT_FN ( INIT_HEAP, init_load_buffer, init_load_buffer, NULL );
		 
#endif
