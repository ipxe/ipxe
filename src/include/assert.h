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

/**
 * Assert a condition at run-time.
 *
 * If the condition is not true, a debug message will be printed.
 * Assertions only take effect in debug-enabled builds (see DBG()).
 *
 * @todo Make an assertion failure abort the program
 *
 */
#define assert( condition ) 						   \
	do { 								   \
		if ( ! (condition) ) { 					   \
			printf ( "assert(%s) failed at %s line %d [%s]\n", \
				 #condition, __FILE__, __LINE__,	   \
				 __FUNCTION__ );			   \
		} 							   \
	} while ( 0 )

/**
 * Assert a condition at link-time.
 *
 * If the condition is not true, the link will fail with an unresolved
 * symbol (error_symbol).
 *
 * This macro is gPXE-specific.  Do not use this macro in code
 * intended to be portable.
 *
 */
#define linker_assert( condition, error_symbol )	\
        if ( ! (condition) ) {				\
                extern void error_symbol ( void );	\
                error_symbol();				\
        }

#ifdef NDEBUG
#undef assert
#define assert(x) do {} while ( 0 )
#endif

#endif /* _ASSERT_H */
