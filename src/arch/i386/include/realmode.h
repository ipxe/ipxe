#ifndef REALMODE_H
#define REALMODE_H

#ifndef ASSEMBLY

#include "stdint.h"
#include "registers.h"
#include "io.h"

/*
 * Data structures and type definitions
 *
 */

/* Segment:offset structure.  Note that the order within the structure
 * is offset:segment.
 */
typedef struct {
	uint16_t offset;
	uint16_t segment;
} __attribute__ (( packed )) segoff_t;

/* Macro hackery needed to stringify bits of inline assembly */
#define RM_XSTR(x) #x
#define RM_STR(x) RM_XSTR(x)

/* Drag in the selected real-mode transition library header */
#ifdef KEEP_IT_REAL
#include "libkir.h"
#else
#include "librm.h"
#endif

/*
 * The API to some functions is identical between librm and libkir, so
 * they are documented here, even though the prototypes are in librm.h
 * and libkir.h.
 *
 */

/*
 * Declaration of variables in .data16
 *
 * To place a variable in the .data16 segment, declare it using the
 * pattern:
 *
 *   int __data16 ( foo );
 *   #define foo __use_data16 ( foo );
 *
 *   extern uint32_t __data16 ( bar );
 *   #define bar __use_data16 ( bar );
 *
 *   extern long __data16 ( baz ) = 0xff000000UL;
 *   #define bar __use_data16 ( baz );
 *
 * i.e. take a normal declaration, add __data16() around the variable
 * name, and add a line saying "#define <name> __use_data16 ( <name> )
 *
 * You can then access them just like any other variable, for example
 *
 *   int x = foo + bar;
 *
 * This magic is achieved at a cost of only around 7 extra bytes per
 * group of accesses to .data16 variables.  When using KEEP_IT_REAL,
 * there is no extra cost.
 *
 * You should place variables in .data16 when they need to be accessed
 * by real-mode code.  Real-mode assembly (e.g. as created by
 * REAL_EXEC()) can access these variables via the usual data segment.
 * You can therefore write something like
 *
 *   static uint16_t __data16 ( foo );
 *   #define foo __use_data16 ( foo )
 *
 *   int bar ( void ) {
 *     REAL_EXEC ( baz,
 *                 "int $0xff\n\t"
 *                 "movw %ax, foo",
 *                 ... );
 *     return foo;
 *   }
 *
 * Variables may also be placed in .text16 using __text16 and
 * __use_text16.  Some variables (e.g. chained interrupt vectors) fit
 * most naturally in .text16; most should be in .data16.
 */

/*
 * void copy_to_real ( uint16_t dest_seg, uint16_t dest_off,
 *		       void *src, size_t n )
 * void copy_from_real ( void *dest, uint16_t src_seg, uint16_t src_off,
 *			 size_t n )
 *
 * These functions can be used to copy data to and from arbitrary
 * locations in base memory.
 */

/*
 * put_real ( variable, uint16_t dest_seg, uint16_t dest_off )
 * get_real ( variable, uint16_t src_seg, uint16_t src_off )
 *
 * These macros can be used to read or write single variables to and
 * from arbitrary locations in base memory.  "variable" must be a
 * variable of either 1, 2 or 4 bytes in length.
 */

/*
 * REAL_CALL ( routine, num_out_constraints, out_constraints,
 *	       in_constraints, clobber )
 * REAL_EXEC ( name, asm_code_str, num_out_constraints, out_constraints,
 *	       in_constraints, clobber )
 *
 * If you have a pre-existing real-mode routine that you want to make
 * a far call to, use REAL_CALL.  If you have a code fragment that you
 * want to copy down to base memory, execute, and then remove, use
 * REAL_EXEC.
 *
 * out_constraints must be of the form OUT_CONSTRAINTS(constraints),
 * and in_constraints must be of the form IN_CONSTRAINTS(constraints),
 * where "constraints" is a constraints list as would be used in an
 * inline __asm__()
 *
 * clobber must be of the form CLOBBER ( clobber_list ), where
 * "clobber_list" is a clobber list as would be used in an inline
 * __asm__().
 *
 * These are best illustrated by example.  To write a character to the
 * console using INT 10, you would do something like:
 *
 *	REAL_EXEC ( rm_test_librm,
 *		    "int $0x10",
 *		    1,
 *		    OUT_CONSTRAINTS ( "=a" ( discard ) ),
 *		    IN_CONSTRAINTS ( "a" ( 0x0e00 + character ),
 *				     "b" ( 1 ) ),
 *		    CLOBBER ( "ebx", "ecx", "edx", "ebp", "esi", "edi" ) );
 *
 * IMPORTANT: gcc does not automatically assume that input operands
 * get clobbered.  The only way to specify that an input operand may
 * be modified is to also specify it as an output operand; hence the
 * "(discard)" in the above code.
 */

#warning "realmode.h contains placeholders for obsolete macros"


/* Just for now */
#define SEGMENT(x) ( virt_to_phys ( x ) >> 4 )
#define OFFSET(x) ( virt_to_phys ( x ) & 0xf )
#define SEGOFF(x) { OFFSET(x), SEGMENT(x) }

/* To make basemem.c compile */
extern int lock_real_mode_stack;
extern char *real_mode_stack;
extern char real_mode_stack_size[];

#define RM_FRAGMENT(name,asm) \
	void name ( void ) {} \
	extern char name ## _size[];



#endif /* ASSEMBLY */

#endif /* REALMODE_H */
