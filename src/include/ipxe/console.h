#ifndef _IPXE_CONSOLE_H
#define _IPXE_CONSOLE_H

#include <stdio.h>
#include <ipxe/tables.h>

/** @file
 *
 * User interaction.
 *
 * Various console devices can be selected via the build options
 * CONSOLE_FIRMWARE, CONSOLE_SERIAL etc.  The console functions
 * putchar(), getchar() and iskey() delegate to the individual console
 * drivers.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/**
 * A console driver
 *
 * Defines the functions that implement a particular console type.
 * Must be made part of the console drivers table by using
 * #__console_driver.
 *
 * @note Consoles that cannot be used before their initialisation
 * function has completed should set #disabled=1 initially.  This
 * allows other console devices to still be used to print out early
 * debugging messages.
 *
 */
struct console_driver {
	/** Console is disabled.
	 *
	 * The console's putchar(), getchar() and iskey() methods will
	 * not be called while #disabled==1.  Typically the console's
	 * initialisation functions will set #disabled=0 upon
	 * completion.
	 *
	 */
	int disabled;

	/** Write a character to the console.
	 *
	 * @v character		Character to be written
	 * @ret None		-
	 * @err None		-
	 *
	 */
	void ( *putchar ) ( int character );

	/** Read a character from the console.
	 *
	 * @v None		-
	 * @ret character	Character read
	 * @err None		-
	 *
	 * If no character is available to be read, this method will
	 * block.  The character read should not be echoed back to the
	 * console.
	 *
	 */
	int ( *getchar ) ( void );

	/** Check for available input.
	 *
	 * @v None		-
	 * @ret True		Input is available
	 * @ret False		Input is not available
	 * @err None		-
	 *
	 * This should return True if a subsequent call to getchar()
	 * will not block.
	 *
	 */
	int ( *iskey ) ( void );

	/** Console usage bitmask
	 *
	 * This is the bitwise OR of zero or more @c CONSOLE_USAGE_XXX
	 * values.
	 */
	int usage;
};

/** Console driver table */
#define CONSOLES __table ( struct console_driver, "consoles" )

/**
 * Mark a <tt> struct console_driver </tt> as being part of the
 * console drivers table.
 *
 * Use as e.g.
 *
 * @code
 *
 *   struct console_driver my_console __console_driver = {
 *      .putchar = my_putchar,
 *	.getchar = my_getchar,
 *	.iskey = my_iskey,
 *   };
 *
 * @endcode
 *
 */
#define __console_driver __table_entry ( CONSOLES, 01 )

/**
 * @defgroup consoleusage Console usages
 * @{
 */

/** Standard output */
#define CONSOLE_USAGE_STDOUT 0x0001

/** Debug messages */
#define CONSOLE_USAGE_DEBUG 0x0002

/** Text-based user interface */
#define CONSOLE_USAGE_TUI 0x0004

/** Log messages */
#define CONSOLE_USAGE_LOG 0x0008

/** All console usages */
#define CONSOLE_USAGE_ALL ( CONSOLE_USAGE_STDOUT | CONSOLE_USAGE_DEBUG | \
			    CONSOLE_USAGE_TUI | CONSOLE_USAGE_LOG )

/** @} */

/**
 * Test to see if console has an explicit usage
 *
 * @v console		Console definition (e.g. CONSOLE_PCBIOS)
 * @ret explicit	Console has an explicit usage
 *
 * This relies upon the trick that the expression ( 2 * N + 1 ) will
 * be valid even if N is defined to be empty, since it will then
 * evaluate to give ( 2 * + 1 ) == ( 2 * +1 ) == 2.
 */
#define CONSOLE_EXPLICIT( console ) ( ( 2 * console + 1 ) != 2 )

extern int console_usage;

/**
 * Set console usage
 *
 * @v usage		New console usage
 * @ret old_usage	Previous console usage
 */
static inline __attribute__ (( always_inline )) int
console_set_usage ( int usage ) {
	int old_usage = console_usage;

	console_usage = usage;
	return old_usage;
}

extern int iskey ( void );
extern int getkey ( unsigned long timeout );

#endif /* _IPXE_CONSOLE_H */
