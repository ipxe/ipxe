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
 *   #define __frobnicator __table ( struct frobnicator, "frobnicators", 01 )
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
 *   // Call all linked-in frobnicators
 *   void frob_all ( void ) {
 *	struct frob *frob;
 *
 *	for_each_table ( frob, "frobnicators" ) {
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
	__section__ ( ".tbl." table "." __table_str ( idx ) )

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
 *   #define __frobnicator __table ( struct frobnicator, "frobnicators", 01 )
 *
 *   struct frobnicator my_frob __frobnicator = {
 *      ...
 *   };
 *
 * @endcode
 *
 */
#define __table( type, table, idx )					\
	__attribute__ (( __table_section ( table, idx ),		\
			 __natural_alignment ( type ) ))

/**
 * Start of linker table.
 *
 * Return the start of a linker table.  Use as e.g.
 *
 * @code
 *
 *   struct frobnicator *frobs =
 *	table_start ( struct frobnicator, "frobnicators" );
 *
 * @endcode
 *
 */
#define table_start( type, table ) ( {					\
	static type __table_start[0] __table ( type, table, 00 );	\
	__table_start; } )

/**
 * End of linker table.
 *
 * Return the end of a linker table.  Use as e.g.
 *
 * @code
 *
 *   struct frobnicator *frobs_end =
 *	table_end ( struct frobnicator, "frobnicators" );
 *
 * @endcode
 *
 */
#define table_end( type, table ) ( {					\
	static type __table_end[0] __table ( type, table, 99 );		\
	__table_end; } )

/**
 * Calculate number of entries in linker table.
 *
 * Return the number of entries within a linker table.  Use as e.g.
 *
 * @code
 *
 *   unsigned int num_frobs =
 *	table_num_entries ( struct frobnicator, "frobnicators" );
 *
 * @endcode
 *
 */
#define table_num_entries( type, table )				\
	( ( unsigned int ) ( table_end ( type, table ) -		\
			     table_start ( type, table ) ) )

/**
 * Iterate through all entries within a linker table.
 *
 * Use as e.g.
 *
 * @code
 *
 *   struct frobnicator *frob;
 *
 *   for_each_table_entry ( frob, "frobnicators" ) {
 *     ...
 *   }
 *
 * @endcode
 *
 */
#define for_each_table_entry( pointer, table )				\
	for ( pointer = table_start ( typeof ( * pointer ), table ) ;	\
	      pointer < table_end ( typeof ( * pointer ), table ) ;	\
	      pointer++ )

/**
 * Iterate through all entries within a linker table in reverse order.
 *
 * Use as e.g.
 *
 * @code
 *
 *   struct frobnicator *frob;
 *
 *   for_each_table_entry_reverse ( frob, "frobnicators" ) {
 *     ...
 *   }
 *
 * @endcode
 *
 */
#define for_each_table_entry_reverse( pointer, table )			\
	for ( pointer = table_end ( typeof ( * pointer ), table ) - 1 ;	\
	      pointer >= table_start ( typeof ( * pointer ), table ) ;	\
	      pointer-- )

#endif /* _GPXE_TABLES_H */
