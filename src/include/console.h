#ifndef CONSOLE_H
#define CONSOLE_H

#include "stdint.h"
#include "vsprintf.h"
#include <gpxe/tables.h>

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

/**
 * A console driver
 *
 * Defines the functions that implement a particular console type.
 * Must be made part of the console drivers table by using
 * #__console_driver.
 *
 * @note Consoles that cannot be used before their INIT_FN() has
 * completed should set #disabled=1 initially.  This allows other
 * console devices to still be used to print out early debugging
 * messages.
 *
 */
struct console_driver {
	/** Console is disabled.
	 *
	 * The console's putchar(), getchar() and iskey() methods will
	 * not be called while #disabled==1.  Typically the
	 * console's initialisation functions (called via INIT_FN())
	 * will set #disabled=0 upon completion.
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
};

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
#define __console_driver __table ( console, 01 )

/* Function prototypes */

extern void putchar ( int character );
extern int getchar ( void );
extern int iskey ( void );

#endif /* CONSOLE_H */
