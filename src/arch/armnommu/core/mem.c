/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "hooks.h"
#include "io.h"
#include "etherboot.h"

struct meminfo meminfo;
void get_memsizes(void)
{
/* We initialize the meminfo structure 
 * according to our development board's specs
 * We do not have a way to automatically probe the 
 * memspace instead we initialize it manually
 */
	meminfo.basememsize = 0x00000000;
	meminfo.memsize     = 0x00008000;
	meminfo.map_count   = 1;

	meminfo.map[0].addr = 0x40000000;
	meminfo.map[0].size = 0x01000000;
	meminfo.map[0].type = E820_RAM;
}
