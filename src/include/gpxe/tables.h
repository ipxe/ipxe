#ifndef _GPXE_TABLES_H
#define _GPXE_TABLES_H

/** @page ifdef_harmful #ifdef considered harmful
 *
 * Overuse of @c #ifdef has long been a problem in Etherboot.
 * Etherboot provides a rich array of features, but all these features
 * take up valuable space in a ROM image.  The traditional solution to
 * this problem has been for each feature to have its own @c #ifdef
 * option, allowing the feature to be compiled in only if desired.
 *
 * The problem with this is that it becomes impossible to compile, let
 * alone test, all possible versions of Etherboot.  Code that is not
 * typically used tends to suffer from bit-rot over time.  It becomes
 * extremely difficult to predict which combinations of compile-time
 * options will result in code that can even compile and link
 * correctly.
 *
 * To solve this problem, we have adopted a new approach from
 * Etherboot 5.5 onwards.  @c #ifdef is now "considered harmful", and
 * its use should be minimised.  Separate features should be
 * implemented in separate @c .c files, and should \b always be
 * compiled (i.e. they should \b not be guarded with a @c #ifdef @c
 * MY_PET_FEATURE statement).  By making (almost) all code always
 * compile, we avoid the problem of bit-rot in rarely-used code.
 *
 * The file config.h, in combination with the @c make command line,
 * specifies the objects that will be included in any particular build
 * of Etherboot.  For example, suppose that config.h includes the line
 *
 * @code
 *
 *   #define CONSOLE_SERIAL
 *   #define DOWNLOAD_PROTO_TFTP
 *
 * @endcode
 *
 * When a particular Etherboot image (e.g. @c bin/rtl8139.zdsk) is
 * built, the options specified in config.h are used to drag in the
 * relevant objects at link-time.  For the above example, serial.o and
 * tftp.o would be linked in.
 *
 * There remains one problem to solve: how do these objects get used?
 * Traditionally, we had code such as
 *
 * @code
 *
 *    #ifdef CONSOLE_SERIAL
 *      serial_init();
 *    #endif
 *
 * @endcode
 *
 * in main.c, but this reintroduces @c #ifdef and so is a Bad Idea.
 * We cannot simply remove the @c #ifdef and make it
 *
 * @code
 *
 *   serial_init();
 *
 * @endcode
 *
 * because then serial.o would end up always being linked in.
 *
 * The solution is to use @link tables.h linker tables @endlink.
 *
 */

/** @file
 *
 * Linker tables
 *
 * Read @ref ifdef_harmful first for some background on the motivation
 * for using linker tables.
 *
 * This file provides macros for dealing with linker-generated tables
 * of fixed-size symbols.  We make fairly extensive use of these in
 * order to avoid @c #ifdef spaghetti and/or linker symbol pollution.
 * For example, instead of having code such as
 *
 * @code
 *
 *    #ifdef CONSOLE_SERIAL
 *      serial_init();
 *    #endif
 *
 * @endcode
 *
 * we make serial.c generate an entry in the initialisation function
 * table, and then have a function call_init_fns() that simply calls
 * all functions present in this table.  If and only if serial.o gets
 * linked in, then its initialisation function will be called.  We
 * avoid linker symbol pollution (i.e. always dragging in serial.o
 * just because of a call to serial_init()) and we also avoid @c
 * #ifdef spaghetti (having to conditionalise every reference to
 * functions in serial.c).
 *
 * The linker script takes care of assembling the tables for us.  All
 * our table sections have names of the format @c .tbl.NAME.NN where
 * @c NAME designates the data structure stored in the table (e.g. @c
 * init_fn) and @c NN is a two-digit decimal number used to impose an
 * ordering upon the tables if required.  @c NN=00 is reserved for the
 * symbol indicating "table start", and @c NN=99 is reserved for the
 * symbol indicating "table end".
 *
 * As an example, suppose that we want to create a "frobnicator"
 * feature framework, and allow for several independent modules to
 * provide frobnicating services.  Then we would create a frob.h
 * header file containing e.g.
 *
 * @code
 *
 *   struct frobnicator {
 *      const char *name;		// Name of the frobnicator
 *	void ( *frob ) ( void ); 	// The frobnicating function itself
 *   };
 *
 *   #define __frobnicator __table ( frobnicators, 01 )
 *
 * @endcode
 *
 * Any module providing frobnicating services would look something
 * like
 *
 * @code
 *
 *   #include "frob.h"
 *
 *   static void my_frob ( void ) {
 *	// Do my frobnicating
 *	...
 *   }
 *
 *   struct frob my_frobnicator __frobnicator = {
 *	.name = "my_frob",
 *	.frob = my_frob,
 *   };
 *
 * @endcode
 *
 * The central frobnicator code (frob.c) would use the frobnicating
 * modules as follows
 *
 * @code
 *
 *   #include "frob.h"
 *
 *   static struct frob frob_start[0] __table_start ( frobnicators );
 *   static struct frob frob_end[0] __table_end ( frobnicators );
 *
 *   // Call all linked-in frobnicators
 *   void frob_all ( void ) {
 *	struct frob *frob;
 *
 *      for ( frob = frob_start ; frob < frob_end ; frob++ ) {
 *         printf ( "Calling frobnicator \"%s\"\n", frob->name );
 *	   frob->frob ();
 *	}
 *   }
 *
 * @endcode
 *
 * See init.h and init.c for a real-life example.
 *
 */

#ifdef DOXYGEN
#define __attribute__( x )
#endif

#define __table_str( x ) #x
#define __table_section( table, idx ) \
	__section__ ( ".tbl." __table_str ( table ) "." __table_str ( idx ) )

#define __table_section_start( table ) __table_section ( table, 00 )
#define __table_section_end( table ) __table_section ( table, 99 )

#define __natural_alignment( type ) __aligned__ ( __alignof__ ( type ) )

/**
 * Linker table entry.
 *
 * Declares a data structure to be part of a linker table.  Use as
 * e.g.
 *
 * @code
 *
 *   struct my_foo __table ( foo, 01 ) = {
 *      ...
 *   };
 *
 * @endcode
 *
 */
#define __table( type, table, idx )				\
	__attribute__ (( __table_section ( table, idx ),	\
			 __natural_alignment ( type ) ))

/**
 * Linker table start marker.
 *
 * Declares a data structure (usually an empty data structure) to be
 * the start of a linker table.  Use as e.g.
 *
 * @code
 *
 *   static struct foo_start[0] __table_start ( foo );
 *
 * @endcode
 *
 */
#define __table_start( type, table ) __table ( type, table, 00 )

/**
 * Linker table end marker.
 *
 * Declares a data structure (usually an empty data structure) to be
 * the end of a linker table.  Use as e.g.
 *
 * @code
 *
 *   static struct foo_end[0] __table_end ( foo );
 *
 * @endcode
 *
 */
#define __table_end( type, table ) __table ( type, table, 99 )

#endif /* _GPXE_TABLES_H */
