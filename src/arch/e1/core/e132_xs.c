/*
 * Copyright 2003 Yannis Mitsos and George Thanos 
 * {gmitsos@gthanos}@telecom.ntua.gr
 * Released under GPL2, see the file COPYING in the top directory
 *
 */
#include "hooks.h"
#include "io.h"
#include "etherboot.h"
#include "e132_xs_board.h"

unsigned int io_periph[NR_CS] = {[0 ... NR_CS-1] = 0 };

/*
void arch_main(struct Elf_Bhdr *ptr __unused)
{

}
*/

void init_peripherals(void)
{
	int i;

	for(i=0; i< NR_CS; i++){
		io_periph[i]= (SLOW_IO_ACCESS | i << 22);
	}

	io_periph[ETHERNET_CS] = (io_periph[ETHERNET_CS] | 1 << IOWait);

	asm volatile("
			ori SR, 0x20
			movi FCR, 0x66ffFFFF"
			:
			:);
}

struct meminfo meminfo;
void get_memsizes(void)
{
/* We initialize the meminfo structure 
 * according to our development board's specs
 * We do not have a way to automatically probe the 
 * memspace instead we initialize it manually
 */
	meminfo.basememsize = BASEMEM;
	meminfo.memsize = 	SDRAM_SIZE;
	meminfo.map_count = NR_MEMORY_REGNS;

	meminfo.map[0].addr = SDRAM_BASEMEM;
	meminfo.map[0].size = SDRAM_SIZE;
	meminfo.map[0].type = E820_RAM;
	meminfo.map[1].addr = SRAM_BASEMEM;
	meminfo.map[1].size = SRAM_SIZE;
	meminfo.map[1].type = E820_RAM;
	meminfo.map[2].addr = IRAM_BASEMEM;
	meminfo.map[2].size = IRAM_SIZE;
	meminfo.map[2].type = E820_RAM;
}

int mach_boot(register unsigned long entry_point)
{
		asm volatile(
					 "mov PC, %0"
					: /* no outputs */
					: "l" (entry_point) );
		return 0; /* We should never reach this point ! */

}

