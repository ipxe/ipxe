/* Etherboot routines for PCBIOS firmware.
 *
 * Body of routines taken from old pcbios.S
 */

#include "realmode.h"
#include "console.h"

#define ZF ( 1 << 6 )

/**************************************************************************
bios_putchar - Print a character on console
**************************************************************************/
static void bios_putchar ( int character ) {
	REAL_EXEC ( rm_console_putc,
		    "sti\n\t"
		    "movb $0x0e, %%ah\n\t"
		    "movl $1, %%ebx\n\t"
		    "int $0x10\n\t"
		    "cli\n\t",
		    1,
		    OUT_CONSTRAINTS ( "=a" ( character ) ),
		    IN_CONSTRAINTS ( "a" ( character ) ),
		    CLOBBER ( "ebx", "ecx", "edx", "ebp", "esi", "edi" ) );

	/* NOTE: %eax may be clobbered, so must be specified as an output
	 * parameter, even though we don't then do anything with it.
	 */
}

/**************************************************************************
bios_getchar - Get a character from console
**************************************************************************/
static int bios_getchar ( void ) {
	uint16_t character;
	
	REAL_EXEC ( rm_console_getc,
		    "sti\n\t"
		    "xorw %%ax, %%ax\n\t"
		    "int $0x16\n\t"
		    "cli\n\t",
		    1,
		    OUT_CONSTRAINTS ( "=a" ( character ) ),
		    IN_CONSTRAINTS (),
		    CLOBBER ( "ebx", "ecx", "edx", "ebp", "esi", "edi" ) );
	
	return ( character & 0xff );
}

/**************************************************************************
bios_iskey - Check for keyboard interrupt
**************************************************************************/
static int bios_iskey ( void ) {
	uint16_t flags;
	
	REAL_EXEC ( rm_console_ischar,
		    "sti\n\t"
		    "movb $1, %%ah\n\t"
		    "int $0x16\n\t"
		    "pushfw\n\t"
		    "popw %%ax\n\t"
		    "cli\n\t",
		    1,
		    OUT_CONSTRAINTS ( "=a" ( flags ) ),
		    IN_CONSTRAINTS (),
		    CLOBBER ( "ebx", "ecx", "edx", "ebp", "esi", "edi" ) );
	
	return ( ( flags & ZF ) == 0 );
}

struct console_driver bios_console __console_driver = {
	.putchar = bios_putchar,
	.getchar = bios_getchar,
	.iskey = bios_iskey,
};
