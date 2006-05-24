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
struct segoff {
	uint16_t offset;
	uint16_t segment;
} __attribute__ (( packed ));

typedef struct segoff segoff_t;

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
 *   static long __data16 ( baz ) = 0xff000000UL;
 *   #define baz __use_data16 ( baz );
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
 * REAL_CODE()) can access these variables via the usual data segment.
 * You can therefore write something like
 *
 *   static uint16_t __data16 ( foo );
 *   #define foo __use_data16 ( foo )
 *
 *   int bar ( void ) {
 *     __asm__ __volatile__ ( REAL_CODE ( "int $0xff\n\t"
 *                                        "movw %ax, foo" )
 *                            : : );
 *     return foo;
 *   }
 *
 * Variables may also be placed in .text16 using __text16 and
 * __use_text16.  Some variables (e.g. chained interrupt vectors) fit
 * most naturally in .text16; most should be in .data16.
 *
 * If you have only a pointer to a magic symbol within .data16 or
 * .text16, rather than the symbol itself, you can attempt to extract
 * the underlying symbol name using __from_data16() or
 * __from_text16().  This is not for the faint-hearted; check the
 * assembler output to make sure that it's doing the right thing.
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
 * REAL_CODE ( asm_code_str )
 *
 * This can be used in inline assembly to create a fragment of code
 * that will execute in real mode.  For example: to write a character
 * to the BIOS console using INT 10, you would do something like:
 *
 *     __asm__ __volatile__ ( REAL_CODE ( "int $0x16" )
 *			      : "=a" ( character ) : "a" ( 0x0000 ) );
 *
 */

#endif /* ASSEMBLY */

#endif /* REALMODE_H */
