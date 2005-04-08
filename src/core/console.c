/*
 * Central console switch.  Various console devices can be selected
 * via the build options CONSOLE_FIRMWARE, CONSOLE_SERIAL etc.
 * config.c picks up on these definitions and drags in the relevant
 * objects.  The linker compiles the console_drivers table for us; we
 * simply delegate to each console_driver that we find in the table.
 *
 * Doing it this way allows for changing CONSOLE_XXX without
 * rebuilding anything other than config.o.  This is extremely useful
 * for rom-o-matic.
 */

#include "stddef.h"
#include "console.h"

/* FIXME: we need a cleaner way to pick up cpu_nap().  It makes a
 * real-mode call, and so we don't want to use it with LinuxBIOS.
 */
#include "bios.h"

extern struct console_driver console_drivers[];
extern struct console_driver console_drivers_end[];

/*****************************************************************************
 * putchar : write a single character to each console
 *****************************************************************************
 */

void putchar ( int character ) {
	struct console_driver *console;

	/* Automatic LF -> CR,LF translation */
	if ( character == '\n' )
		putchar ( '\r' );

	for ( console = console_drivers; console < console_drivers_end ;
	      console++ ) {
		if ( ( ! console->disabled ) && console->putchar )
			console->putchar ( character );
	}
}

/*****************************************************************************
 * has_input : check to see if any input is available on any console,
 * and return a pointer to the console device if so
 *****************************************************************************
 */
static struct console_driver * has_input ( void ) {
	struct console_driver *console;

	for ( console = console_drivers; console < console_drivers_end ;
	      console++ ) {
		if ( ( ! console->disabled ) && console->iskey ) {
			if ( console->iskey () )
				return console;
		}
	}
	return NULL;
}

/*****************************************************************************
 * getchar : read a single character from any console
 *
 * NOTE : this function does not echo the character, and it does block
 *****************************************************************************
 */

int getchar ( void ) {
	struct console_driver *console;
	int character = 256;

	while ( character == 256 ) {
		/* Doze for a while (until the next interrupt).  This works
		 * fine, because the keyboard is interrupt-driven, and the
		 * timer interrupt (approx. every 50msec) takes care of the
		 * serial port, which is read by polling.  This reduces the
		 * power dissipation of a modern CPU considerably, and also
		 * makes Etherboot waiting for user interaction waste a lot
		 * less CPU time in a VMware session.
		 */
		cpu_nap();
		
		console = has_input();
		if ( console && console->getchar )
			character = console->getchar ();
	}

	/* CR -> LF translation */
	if ( character == '\r' )
		character = '\n';

	return character;
}

/*****************************************************************************
 * iskey : check to see if any input is available on any console
 *****************************************************************************
 */

int iskey ( void ) {
	return has_input() ? 1 : 0;
}
