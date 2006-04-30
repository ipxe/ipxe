#include "stdint.h"
#include "stddef.h"
#include "realmode.h"
#include <gpxe/init.h>
#include "etherboot.h"
#include "memsizes.h"

#define CF ( 1 << 0 )

/* by Eric Biederman */

struct meminfo meminfo;

/**************************************************************************
BASEMEMSIZE - Get size of the conventional (base) memory
**************************************************************************/
static unsigned short basememsize ( void ) {
	uint16_t int12_basememsize, fbms_basememsize;
	uint16_t basememsize;

	/* There are two methods for retrieving the base memory size:
	 * INT 12 and the BIOS FBMS counter at 40:13.  We read both
	 * and use the smaller value, to be paranoid.
	 * 
	 * We then store the smaller value in the BIOS FBMS counter so
	 * that other code (e.g. basemem.c) can rely on it and not
	 * have to use INT 12.  This is especially important because
	 * basemem.c functions can be called in a context in which
	 * there is no real-mode stack (e.g. when trying to allocate
	 * memory for a real-mode stack...)
	 */

	REAL_EXEC ( rm_basememsize,
		    "int $0x12\n\t",
		    1,
		    OUT_CONSTRAINTS ( "=a" ( int12_basememsize ) ),
		    IN_CONSTRAINTS (),
		    CLOBBER ( "ebx", "ecx", "edx", "ebp", "esi", "edi" ) );

	get_real ( fbms_basememsize, 0x40, 0x13 );

	basememsize = ( int12_basememsize < fbms_basememsize ?
			int12_basememsize : fbms_basememsize );

	put_real ( basememsize, 0x40, 0x13 );

	return basememsize;
}

/**************************************************************************
MEMSIZE - Determine size of extended memory, in kB
**************************************************************************/
static unsigned int memsize ( void ) {
	uint16_t extmem_1m_to_16m_k, extmem_16m_plus_64k;
	uint16_t confmem_1m_to_16m_k, confmem_16m_plus_64k;
	uint16_t flags;
	int memsize;

	/* Try INT 15,e801 first
	 *
	 * Some buggy BIOSes don't clear/set carry on pass/error of
	 * e801h memory size call or merely pass cx,dx through without
	 * changing them, so we set carry and zero cx,dx before call.
	 */
	REAL_EXEC ( rm_mem_e801,
		    "stc\n\t"
		    "int $0x15\n\t"
		    "pushfw\n\t"	/* flags -> %di */
		    "popw %%di\n\t",
		    5,
		    OUT_CONSTRAINTS ( "=a" ( extmem_1m_to_16m_k ),
				      "=b" ( extmem_16m_plus_64k ),
				      "=c" ( confmem_1m_to_16m_k ),
				      "=d" ( confmem_16m_plus_64k ),
				      "=D" ( flags ) ),
		    IN_CONSTRAINTS ( "a" ( 0xe801 ),
				     "c" ( 0 ),
				     "d" ( 0 ) ),
		    CLOBBER ( "ebp", "esi" ) );

	if ( ! ( flags & CF ) ) {
		/* INT 15,e801 succeeded */
		if ( confmem_1m_to_16m_k || confmem_16m_plus_64k ) {
			/* Use confmem (cx,dx) values */
			memsize = confmem_1m_to_16m_k +
				( confmem_16m_plus_64k << 6 );
		} else {
			/* Use extmem (ax,bx) values */
			memsize = extmem_1m_to_16m_k +
				( extmem_16m_plus_64k << 6 );
		}
	} else {
		/* INT 15,e801 failed; fall back to INT 15,88
		 *
		 * CF is apparently unreliable and should be ignored.
		 */
		REAL_EXEC ( rm_mem_88,
			    "int $0x15\n\t",
			    1,
			    OUT_CONSTRAINTS ( "=a" ( extmem_1m_to_16m_k ) ),
			    IN_CONSTRAINTS ( "a" ( 0x88 << 8 ) ),
			    CLOBBER ( "ebx", "ecx", "edx",
				      "ebp", "esi", "edi" ) );
		memsize = extmem_1m_to_16m_k;
	}

	return memsize;
}

/**************************************************************************
MEME820 - Retrieve the E820 BIOS memory map
**************************************************************************/
#define SMAP ( 0x534d4150 ) /* "SMAP" */
static int meme820 ( struct e820entry *buf, int count ) {
	int index;
	uint16_t basemem_entry;
	uint32_t smap, next;
	uint16_t flags;
	uint32_t discard_c, discard_d;

	index = 0;
	next = 0;
	do {
		basemem_entry = BASEMEM_PARAMETER_INIT ( buf[index] );
		REAL_EXEC ( rm_mem_e820,
			    "int $0x15\n\t"
			    "pushfw\n\t"	/* flags -> %di */
			    "popw %%di\n\t",
			    5,
			    OUT_CONSTRAINTS ( "=a" ( smap ),
					      "=b" ( next ),
					      "=c" ( discard_c ),
					      "=d" ( discard_d ),
					      "=D" ( flags ) ),
			    IN_CONSTRAINTS ( "a" ( 0xe820 ),
					     "b" ( next ),
					     "c" ( sizeof (struct e820entry) ),
					     "d" ( SMAP ),
					     "D" ( basemem_entry ) ),
			    CLOBBER ( "ebp", "esi" ) );
		BASEMEM_PARAMETER_DONE ( buf[index] );
		if ( smap != SMAP ) return 0;
		if ( flags & CF ) break;
		index++;
	} while ( ( index < count ) && ( next != 0 ) );
		
	return index;
}

/**************************************************************************
GET_MEMSIZES - Retrieve the system memory map via any available means
**************************************************************************/
void get_memsizes ( void ) {
	/* Ensure we don't stomp bios data structutres.
	 * the interrupt table: 0x000 - 0x3ff
	 * the bios data area:  0x400 - 0x502
	 * Dos variables:       0x502 - 0x5ff
	 */
	static const unsigned min_addr = 0x600;
	unsigned i;
	unsigned basemem;

	/* Retrieve memory information from the BIOS */
	meminfo.basememsize = basememsize();
	basemem = meminfo.basememsize << 10;
	meminfo.memsize = memsize();
#ifndef IGNORE_E820_MAP
	meminfo.map_count = meme820 ( meminfo.map, E820MAX );
#else
	meminfo.map_count = 0;
#endif

	/* If we don't have an e820 memory map fake it */
	if ( meminfo.map_count == 0 ) {
		meminfo.map_count = 2;
		meminfo.map[0].addr = 0;
		meminfo.map[0].size = meminfo.basememsize << 10;
		meminfo.map[0].type = E820_RAM;
		meminfo.map[1].addr = 1024*1024;
		meminfo.map[1].size = meminfo.memsize << 10;
		meminfo.map[1].type = E820_RAM;
	}

	/* Scrub the e820 map */
	for ( i = 0; i < meminfo.map_count; i++ ) {
		if ( meminfo.map[i].type != E820_RAM ) {
			continue;
		}

		/* Reserve the bios data structures */
		if ( meminfo.map[i].addr < min_addr ) {
			unsigned long delta;
			delta = min_addr - meminfo.map[i].addr;
			if ( delta > meminfo.map[i].size ) {
				delta = meminfo.map[i].size;
			}
			meminfo.map[i].addr = min_addr;
			meminfo.map[i].size -= delta;
		}

		/* Ensure the returned e820 map is in sync with the
		 * actual memory state
		 */
		if ( ( meminfo.map[i].addr < 0xa0000 ) && 
		     (( meminfo.map[i].addr+meminfo.map[i].size ) > basemem )){
			if ( meminfo.map[i].addr <= basemem ) {
				meminfo.map[i].size = basemem
					- meminfo.map[i].addr;
			} else {
				meminfo.map[i].addr = basemem;
				meminfo.map[i].size = 0;
			}
		}
	}

#ifdef DEBUG_MEMSIZES
	printf ( "basememsize %d\n", meminfo.basememsize );
	printf ( "memsize %d\n",     meminfo.memsize );
	printf ( "Memory regions(%d):\n", meminfo.map_count );
	for ( i = 0; i < meminfo.map_count; i++ ) {
		unsigned long long r_start, r_end;
		r_start = meminfo.map[i].addr;
		r_end = r_start + meminfo.map[i].size;
		printf ( "[%X%X, %X%X) type %d\n", 
			 ( unsigned long ) ( r_start >> 32 ),
			 ( unsigned long ) r_start,
			 ( unsigned long ) ( r_end >> 32 ),
			 ( unsigned long ) r_end,
			 meminfo.map[i].type );
	}
#endif

}

INIT_FN ( INIT_MEMSIZES, get_memsizes, NULL, NULL );
