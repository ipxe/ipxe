#ifndef _ASSERT_H
#define _ASSERT_H

/** @file
 *
 * Assertions
 *
 * This file provides two assertion macros: assert() (for run-time
 * assertions) and linker_assert() (for link-time assertions).
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifndef ASSERTING
#ifdef NDEBUG
#define ASSERTING 0
#else
#define ASSERTING 1
#endif
#endif

extern unsigned int assertion_failures;

#define ASSERTED ( ASSERTING && ( assertion_failures != 0 ) )

/** printf() for assertions
 *
 * This function exists so that the assert() macro can expand to
 * printf() calls without dragging the printf() prototype into scope.
 *
 * As far as the compiler is concerned, assert_printf() and printf() are
 * completely unrelated calls; it's only at the assembly stage that
 * references to the assert_printf symbol are collapsed into references
 * to the printf symbol.
 */
extern int __attribute__ (( format ( printf, 1, 2 ) )) 
assert_printf ( const char *fmt, ... ) asm ( "printf" );

/**
 * Assert a condition at run-time.
 *
 * If the condition is not true, a debug message will be printed.
 * Assertions only take effect in debug-enabled builds (see DBG()).
 *
 * @todo Make an assertion failure abort the program
 *
 */
#define assert( condition ) 						     \
	do { 								     \
		if ( ASSERTING && ! (condition) ) { 			     \
			assert_printf ( "assert(%s) failed at %s line %d\n", \
					#condition, __FILE__, __LINE__ );    \
			assertion_failures++;				     \
		} 							     \
	} while ( 0 )

/**
 * Assert a condition at build time
 *
 * If the compiler cannot prove that the condition is true, the build
 * will fail with an error message.
 */
#undef static_assert
#define static_assert(x) _Static_assert( x, #x )

/**
 * Assert a condition at build time (after dead code elimination)
 *
 * If the compiler cannot prove that the condition is true, the build
 * will fail with an error message.
 *
 * This macro is iPXE-specific.  Do not use this macro in code
 * intended to be portable.
 */
#define build_assert( condition )					     \
	do { 								     \
		if ( ! (condition) ) {					     \
			extern void __attribute__ (( warning (		     \
				"build_assert(" #condition ") failed"	     \
			) )) _C2 ( build_assert_, __LINE__ ) ( void );	     \
			_C2 ( build_assert_, __LINE__ ) ();		     \
		}							     \
	} while ( 0 )

#endif /* _ASSERT_H */
