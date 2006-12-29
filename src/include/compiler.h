#ifndef COMPILER_H
#define COMPILER_H

/*
 * Doxygen can't cope with some of the more esoteric areas of C, so we
 * make its life simpler.
 *
 */
#ifdef DOXYGEN
#define __attribute__(x)
#endif

/** @file
 *
 * Global compiler definitions.
 *
 * This file is implicitly included by every @c .c file in Etherboot.
 * It defines global macros such as DBG().
 *
 * We arrange for each object to export the symbol @c obj_OBJECT
 * (where @c OBJECT is the object name, e.g. @c rtl8139) as a global
 * symbol, so that the linker can drag in selected object files from
 * the library using <tt> -u obj_OBJECT </tt>.
 *
 */

/* Not quite sure why cpp requires two levels of macro call in order
 * to actually expand OBJECT...
 */
#undef _H1
#define _H1( x, y ) x ## y
#undef _H2
#define _H2( x, y ) _H1 ( x, y )
#define PREFIX_OBJECT(prefix) _H2 ( prefix, OBJECT )
#define OBJECT_SYMBOL PREFIX_OBJECT(obj_)
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

/**
 * Drag in an object by object name.
 *
 * Macro to allow objects to explicitly drag in other objects by
 * object name.  Used by config.c.
 *
 */
#define REQUIRE_OBJECT(object) \
	__asm__ ( ".equ\tneed_" #object ", obj_" #object );

/** @def DBG
 *
 * Print a debugging message.
 *
 * The debug level is set at build time by specifying the @c DEBUG=
 * parameter on the @c make command line.  For example, to enable
 * debugging for the PCI bus functions (in pci.c) in a @c .dsk image
 * for the @c rtl8139 card, you could use the command line
 *
 * @code
 *
 *   make bin/rtl8139.dsk DEBUG=pci
 *
 * @endcode
 *
 * This will enable the debugging statements (DBG()) in pci.c.  If
 * debugging is not enabled, DBG() statements will be ignored.
 *
 * You can enable debugging in several objects simultaneously by
 * separating them with commas, as in
 *
 * @code
 *
 *   make bin/rtl8139.dsk DEBUG=pci,buffer,heap
 *
 * @endcode
 *
 * You can increase the debugging level for an object by specifying it
 * with @c :N, where @c N is the level, as in
 *
 * @code
 *
 *   make bin/rtl8139.dsk DEBUG=pci,buffer:2,heap
 *
 * @endcode
 *
 * which would enable debugging for the PCI, buffer-handling and
 * heap-allocation code, with the buffer-handling code at level 2.
 *
 */

/*
 * If debug_OBJECT is set to a true value, the macro DBG(...) will
 * expand to printf(...) when compiling OBJECT, and the symbol
 * DEBUG_LEVEL will be inserted into the object file.
 *
 */
#define DEBUG_SYMBOL PREFIX_OBJECT(debug_)

#if DEBUG_SYMBOL
#include "console.h"
#define DEBUG_SYMBOL_STR _XSTR ( DEBUG_SYMBOL )
__asm__ ( ".equ\tDBGLVL, " DEBUG_SYMBOL_STR );
#endif

/** printf() for debugging
 *
 * This function exists so that the DBG() macros can expand to
 * printf() calls without dragging the printf() prototype into scope.
 *
 * As far as the compiler is concerned, dbg_printf() and printf() are
 * completely unrelated calls; it's only at the assembly stage that
 * references to the dbg_printf symbol are collapsed into references
 * to the printf symbol.
 */
extern int __attribute__ (( format ( printf, 1, 2 ) )) 
dbg_printf ( const char *fmt, ... ) asm ( "printf" );

extern void __attribute__ (( format ( printf, 2, 3 ) )) 
dbg_printf_autocolour ( void *id, const char *fmt, ... );

/* Compatibility with existing Makefile */
#if DEBUG_SYMBOL >= 1
#if DEBUG_SYMBOL >= 2
#define DBGLVL 3
#else
#define DBGLVL 1
#endif
#else
#define DBGLVL 0
#endif

#define DBGLVL_LOG	1
#define DBG_LOG		( DBGLVL & DBGLVL_LOG )
#define DBGLVL_EXTRA	2
#define DBG_EXTRA	( DBGLVL & DBGLVL_EXTRA )

#define DBG_IF( level, ... ) do {				\
		if ( DBG_ ## level ) {				\
			dbg_printf ( __VA_ARGS__ );		\
		}						\
	} while ( 0 )

#define DBGC_IF( level, ... ) do {				\
		if ( DBG_ ## level ) {				\
			dbg_printf_autocolour ( __VA_ARGS__ );	\
		}						\
	} while ( 0 )

#define DBG( ... )	DBG_IF ( LOG, __VA_ARGS__ )
#define DBG2( ... )	DBG_IF ( EXTRA, __VA_ARGS__ )
#define DBGC( ... )	DBGC_IF ( LOG, __VA_ARGS__ )

#if DEBUG_SYMBOL == 0
#define NDEBUG
#endif

/** Declare a data structure as packed. */
#define PACKED __attribute__ (( packed ))

/** Declare a variable or data structure as unused. */
#define __unused __attribute__ (( unused ))

/**
 * Declare a function as used.
 *
 * Necessary only if the function is called only from assembler code.
 */
#define __used __attribute__ (( used ))

/** Declare a data structure to be aligned with 16-byte alignment */
#define __aligned __attribute__ (( aligned ( 16 ) ))

/**
 * Shared data.
 *
 * To save space in the binary when multiple-driver images are
 * compiled, uninitialised data areas can be shared between drivers.
 * This will typically be used to share statically-allocated receive
 * and transmit buffers between drivers.
 *
 * Use as e.g.
 *
 * @code
 *
 *   struct {
 *	char	rx_buf[NUM_RX_BUF][RX_BUF_SIZE];
 *	char	tx_buf[TX_BUF_SIZE];
 *   } my_static_data __shared;
 *
 * @endcode
 *
 */
#define __shared __asm__ ( "_shared_bss" )

#endif /* ASSEMBLY */

#endif /* COMPILER_H */
