/* Real-mode interface: C portions.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 */

#include "realmode.h"

/*
 * Copy data to/from base memory.
 *
 */

#ifdef KEEP_IT_REAL

void memcpy_to_real ( segoff_t dest, void *src, size_t n ) {

}

void memcpy_from_real ( void *dest, segoff_t src, size_t n ) {

}

#endif /* KEEP_IT_REAL */


#define RM_STACK_SIZE ( 0x1000 )

/* gcc won't let us use extended asm outside a function (compiler
 * bug), ao we have to put these asm statements inside a dummy
 * function.
 */
static void work_around_gcc_bug ( void ) __attribute__ ((used));
static void work_around_gcc_bug ( void ) {
	/* Export _real_mode_stack_size as absolute linker symbol */
	__asm__ ( ".globl real_mode_stack_size" );
	__asm__ ( ".equ real_mode_stack_size, %c0" : : "i" (RM_STACK_SIZE) );
}

char *real_mode_stack;
int lock_real_mode_stack;
