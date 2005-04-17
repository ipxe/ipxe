#ifndef COMPILER_H
#define COMPILER_H

/* We export the symbol obj_OBJECT (OBJECT is defined on command-line)
 * as a global symbol, so that the linker can drag in selected object
 * files from the library using -u obj_OBJECT.
 *
 * Not quite sure why cpp requires two levels of macro call in order
 * to actually expand OBJECT...
 */
#undef _H1
#define _H1( x, y ) x ## y
#undef _H2
#define _H2( x, y ) _H1 ( x, y )
#define OBJECT_SYMBOL _H2 ( obj_, OBJECT )
#undef _STR
#define _STR(s) #s
#undef _XSTR
#define _XSTR(s) _STR(s)
#define OBJECT_SYMBOL_STR _XSTR ( OBJECT_SYMBOL )

#ifdef ASSEMBLY

	.globl	OBJECT_SYMBOL
	.equ	OBJECT_SYMBOL, 0

#else /* ASSEMBLY */

__asm__ ( ".globl\t" OBJECT_SYMBOL_STR );
__asm__ ( ".equ\t" OBJECT_SYMBOL_STR ", 0" );

/*
 * Macro to allow objects to explicitly drag in other objects by
 * object name.  Used by config.c.
 *
 */
#define REQUIRE_OBJECT(object) \
	__asm__ ( ".equ\tneed_" #object ", obj_" #object );

/*
 * If debug_OBJECT is set to a true value, the macro DBG(...) will
 * expand to printf(...) when compiling OBJECT, and the symbol
 * WITH_DEBUG_MESSAGES will be inserted into the object file.
 *
 */
#define DEBUG_SYMBOL _H2 ( debug_, OBJECT )
#if DEBUG_SYMBOL
#include "console.h"
#define DBG(...) printf ( __VA_ARGS__ )
#define DEBUG_SYMBOL_STR _XSTR ( DEBUG_SYMBOL )
__asm__ ( ".equ\tWITH_DEBUG_MESSAGES, 0" );
#else
#define DBG(...)
#endif

#define PACKED __attribute__((packed))
#define __unused __attribute__((unused))
#define __used __attribute__((used))

#endif /* ASSEMBLY */

#endif /* COMPILER_H */
