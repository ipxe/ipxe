#ifdef PCBIOS

#include "etherboot.h"
#include "realmode.h"

#define CF ( 1 << 0 )

#ifndef MEMSIZES_DEBUG 
#define MEMSIZES_DEBUG 0
#endif

/* by Eric Biederman */

struct meminfo meminfo;

/**************************************************************************
BASEMEMSIZE - Get size of the conventional (base) memory
**************************************************************************/
unsigned short basememsize ( void )
{
	RM_FRAGMENT(rm_basememsize,
		"int $0x12\n\t"
	);	
	return real_call ( rm_basememsize, NULL, NULL );
}

/**************************************************************************
MEMSIZE - Determine size of extended memory
**************************************************************************/
unsigned int memsize ( void )
{
	struct {
		reg16_t ax;
	} PACKED in_stack;
	struct {
		reg16_t ax;
		reg16_t bx;
		reg16_t cx;
		reg16_t dx;
		reg16_t flags;
	} PACKED out_stack;
	int memsize;

	RM_FRAGMENT(rm_memsize,
	/* Some buggy BIOSes don't clear/set carry on pass/error of
	 * e801h memory size call or merely pass cx,dx through without
	 * changing them, so we set carry and zero cx,dx before call.
	 */
		"stc\n\t"
		"xorw %cx,%cx\n\t"
		"xorw %dx,%dx\n\t"
		"popw %ax\n\t"
		"int $0x15\n\t"
		"pushfw\n\t"
		"pushw %dx\n\t"
		"pushw %cx\n\t"
		"pushw %bx\n\t"
		"pushw %ax\n\t"
	);

	/* Try INT 15,e801 first */
	in_stack.ax.word = 0xe801;
	real_call ( rm_memsize, &in_stack, &out_stack );
	if ( out_stack.flags.word & CF ) {
		/* INT 15,e801 not supported: try INT 15,88 */
		in_stack.ax.word = 0x8800;
		memsize = real_call ( rm_memsize, &in_stack, &out_stack );
	} else {
		/* Some BIOSes report extended memory via ax,bx rather
		 * than cx,dx
		 */
		if ( (out_stack.cx.word==0) && (out_stack.dx.word==0) ) {
			/* Use ax,bx */
			memsize = ( out_stack.bx.word<<6 ) + out_stack.ax.word;
		} else {
			/* Use cx,dx */
			memsize = ( out_stack.dx.word<<6 ) + out_stack.cx.word;
		}
	}
	return memsize;
}

#define SMAP ( 0x534d4150 )
int meme820 ( struct e820entry *buf, int count )
{
	struct {
		reg16_t flags;
		reg32_t eax;
		reg32_t ebx;
		struct e820entry entry;
	} PACKED stack;
	int index = 0;

	RM_FRAGMENT(rm_meme820,
		"addw $6, %sp\n\t"	/* skip flags, eax */
		"popl %ebx\n\t"
		"pushw %ss\n\t"	/* es:di = ss:sp */
		"popw %es\n\t"
		"movw %sp, %di\n\t"
		"movl $0xe820, %eax\n\t"
		"movl $" RM_STR(SMAP) ", %edx\n\t"
		"movl $" RM_STR(E820ENTRY_SIZE) ", %ecx\n\t"
		"int $0x15\n\t"
		"pushl %ebx\n\t"
		"pushl %eax\n\t"
		"pushfw\n\t"
	);

	stack.ebx.dword = 0; /* 'EOF' marker */
	while ( ( index < count ) &&
		( ( index == 0 ) || ( stack.ebx.dword != 0 ) ) ) {
		real_call ( rm_meme820, &stack, &stack );
		if ( stack.eax.dword != SMAP ) return 0;
		if ( stack.flags.word & CF ) return 0;
		buf[index++] = stack.entry;
	}
	return index;
}

void get_memsizes(void)
{
	/* Ensure we don't stomp bios data structutres.
	 * the interrupt table: 0x000 - 0x3ff
	 * the bios data area:  0x400 - 0x502
	 * Dos variables:       0x502 - 0x5ff
	 */
	static const unsigned min_addr = 0x600;
	unsigned i;
	unsigned basemem;
	basemem = get_free_base_memory();
	meminfo.basememsize = basememsize();
	meminfo.memsize = memsize();
#ifndef IGNORE_E820_MAP
	meminfo.map_count = meme820(meminfo.map, E820MAX);
#else
	meminfo.map_count = 0;
#endif
	if (meminfo.map_count == 0) {
		/* If we don't have an e820 memory map fake it */
		meminfo.map_count = 2;
		meminfo.map[0].addr = 0;
		meminfo.map[0].size = meminfo.basememsize << 10;
		meminfo.map[0].type = E820_RAM;
		meminfo.map[1].addr = 1024*1024;
		meminfo.map[1].size = meminfo.memsize << 10;
		meminfo.map[1].type = E820_RAM;
	}
	/* Scrub the e820 map */
	for(i = 0; i < meminfo.map_count; i++) {
		if (meminfo.map[i].type != E820_RAM) {
			continue;
		}
		/* Reserve the bios data structures */
		if (meminfo.map[i].addr < min_addr) {
			unsigned long delta;
			delta = min_addr - meminfo.map[i].addr;
			if (delta > meminfo.map[i].size) {
				delta = meminfo.map[i].size;
			}
			meminfo.map[i].addr = min_addr;
			meminfo.map[i].size -= delta;
		}
		/* Ensure the returned e820 map is in sync 
		 * with the actual memory state 
		 */
		if ((meminfo.map[i].addr < 0xa0000) && 
			((meminfo.map[i].addr + meminfo.map[i].size) > basemem))
		{
			if (meminfo.map[i].addr <= basemem) {
				meminfo.map[i].size = basemem - meminfo.map[i].addr;
			} else {
				meminfo.map[i].addr = basemem;
				meminfo.map[i].size = 0;
			}
		}
	}
#if MEMSIZES_DEBUG
{
	int i;
	printf("basememsize %d\n", meminfo.basememsize);
	printf("memsize %d\n",     meminfo.memsize);
	printf("Memory regions(%d):\n", meminfo.map_count);
	for(i = 0; i < meminfo.map_count; i++) {
		unsigned long long r_start, r_end;
		r_start = meminfo.map[i].addr;
		r_end = r_start + meminfo.map[i].size;
		printf("[%X%X, %X%X) type %d\n", 
			(unsigned long)(r_start >> 32),
			(unsigned long)r_start,
			(unsigned long)(r_end >> 32),
			(unsigned long)r_end,
			meminfo.map[i].type);
#if defined(CONSOLE_FIRMWARE)
		sleep(1); /* No way to see 32 entries on a standard 80x25 screen... */
#endif
	}
}
#endif
}

#endif /* PCBIOS */
