/*
 * Copyright 2003 Yannis Mitsos and George Thanos 
 * {gmitsos@gthanos}@telecom.ntua.gr
 * Released under GPL2, see the file COPYING in the top directory
 *
 */
#include "setjmp.h"

int setjmp( jmp_buf state)
{
	asm volatile(	"mov %0, G3\n\t"           
			"mov %1, G4\n\t" 
			:"=l"(state->__jmpbuf->G3), 
			 "=l"(state->__jmpbuf->G4) 
			:/*no input*/ 
			:"%G3", "%G4" );

	asm volatile(   "setadr  %0\n\t"
			"mov %1, L1\n\t"
			"mov %2, L2\n\t"
			:"=l"(state->__jmpbuf->SavedSP),
			 "=l"(state->__jmpbuf->SavedPC),
			 "=l"(state->__jmpbuf->SavedSR)
			:/*no input*/);
	return 0;
}
