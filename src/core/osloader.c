/**************************************************************************
OS loader

Author: Markus Gutschke (gutschk@math.uni-muenster.de)
  Date: Sep/95
Modifications: Ken Yap (for Etherboot/16)
  Doug Ambrisko (ELF and a.out support)
  Klaus Espenlaub (rewrote ELF and a.out (did it really work before?) support,
      added ELF Multiboot images).  Someone should merge the ELF and a.out
      loaders, as most of the code is now identical.  Maybe even NBI could be
      rewritten and merged into the generic loading framework.  This should
      save quite a few bytes of code if you have selected more than one format.
  Ken Yap (Jan 2001)
      Added support for linear entry addresses in tagged images,
      which allows a more efficient protected mode call instead of
      going to real mode and back. Also means entry addresses > 1 MB can
      be called.  Conditional on the LINEAR_EXEC_ADDR bit.
      Added support for Etherboot extension calls. Conditional on the
      TAGGED_PROGRAM_RETURNS bit. Implies LINEAR_EXEC_ADDR.
      Added support for non-MULTIBOOT ELF which also supports Etherboot
      extension calls. Conditional on the ELF_PROGRAM_RETURNS bit.

**************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include "stdio.h"
#include "io.h"
#include "memsizes.h"

/* Linker symbols */
extern char _text[];
extern char _end[];

int prep_segment ( physaddr_t start, physaddr_t mid, physaddr_t end ) {
	unsigned fit, i;

	DBG ( "OSLOADER preparing segment [%lX,%lX)\n", start, end );

	if ( mid > end ) {
		DBG ( "OSLOADER got filesz > memsz\n" );
		return 0;
	}

	/* Check for overlap with Etherboot runtime image */
	if ( ( end > virt_to_phys ( _text ) ) && 
	     ( start < virt_to_phys ( _end ) ) ) {
		DBG ( "OSLOADER got segment [%lX, %lX) "
		      "overlapping etherboot [%lX, %lX)\n",
		      start, end,
		      virt_to_phys ( _text ), virt_to_phys ( _end ) );
		return 0;
	}

	/* Check that block fits entirely inside a single memory region */
	fit = 0;
	for ( i = 0 ; i < meminfo.map_count ; i++ ) {
		unsigned long long r_start, r_end;

		if (meminfo.map[i].type != E820_RAM)
			continue;

		r_start = meminfo.map[i].addr;
		r_end = r_start + meminfo.map[i].size;
		if ( ( start >= r_start ) && ( end <= r_end ) ) {
			fit = 1;
			break;
		}
	}
	if ( ! fit ) {
		DBG ( "OSLOADER got segment [%lX,%lX) "
		      "which does not fit in any memory region\n",
			start, end );
		return 0;
	}

	/* Zero the bss */
	memset ( phys_to_virt ( mid ), 0, end - mid );

	return 1;
}

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
