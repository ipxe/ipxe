/*
 * Copyright 2003 Yannis Mitsos and George Thanos 
 * {gmitsos@gthanos}@telecom.ntua.gr
 * Released under GPL2, see the file COPYING in the top directory
 *
 */
#include "setjmp.h"

unsigned long jmpbuf_ptr;

void longjmp(jmp_buf state, int value )
{
	if(!value)
		state->__jmpbuf->ReturnValue = 1;
	else
		state->__jmpbuf->ReturnValue = value;

	jmpbuf_ptr = (unsigned long)state; 

#define _state_ ((struct __jmp_buf_tag*)jmpbuf_ptr)
	asm volatile("mov L0, %0\n\t"
		     "mov L1, %1\n\t"
		     "mov L2, %2\n\t"
		     "mov G3, %3\n\t"
		     "mov G4, %4\n\t"
		     "ret PC, L1\n\t"
		     :/*no output*/
		     :"l"(_state_->__jmpbuf->ReturnValue),
		      "l"(_state_->__jmpbuf->SavedPC),
		      "l"(_state_->__jmpbuf->SavedSR),
		      "l"(_state_->__jmpbuf->G3),
		      "l"(_state_->__jmpbuf->G4)
		     :"%G3", "%G4", "%L0", "%L1" );
#undef _state_
}
