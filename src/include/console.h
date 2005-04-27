#ifndef CONSOLE_H
#define CONSOLE_H

#include "stdint.h"
#include "vsprintf.h"
#include "tables.h"

/*
 * Consoles that cannot be used before their INIT_FN() has completed
 * should set disabled = 1 initially.  This allows other console
 * devices to still be used to print out early debugging messages.
 */

struct console_driver {
	int disabled;
	void ( *putchar ) ( int character );
	int ( *getchar ) ( void );
	int ( *iskey ) ( void );
};

#define __console_driver \
	__attribute__ (( used, __table_section ( console, 01 ) ))

/* Function prototypes */

extern void putchar ( int character );
extern int getchar ( void );
extern int iskey ( void );

#endif /* CONSOLE_H */
