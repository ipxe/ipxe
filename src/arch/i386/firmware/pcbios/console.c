/* Etherboot routines for PCBIOS firmware.
 *
 * Body of routines taken from old pcbios.S
 */

#ifdef PCBIOS

#include "etherboot.h"
#include "realmode.h"
#include "segoff.h"

#define ZF ( 1 << 6 )

/**************************************************************************
CONSOLE_PUTC - Print a character on console
**************************************************************************/
void console_putc ( int character )
{
	struct {
		reg16_t ax;
	} PACKED in_stack;

	RM_FRAGMENT(rm_console_putc,
		"sti\n\t"
		"popw %ax\n\t"
		"movb $0x0e, %ah\n\t"
		"movl $1, %ebx\n\t"
		"int $0x10\n\t"
		"cli\n\t"
	);

	in_stack.ax.l = character;
	real_call ( rm_console_putc, &in_stack, NULL );
}

/**************************************************************************
CONSOLE_GETC - Get a character from console
**************************************************************************/
int console_getc ( void )
{
	RM_FRAGMENT(rm_console_getc,
		"sti\n\t"
		"xorw %ax, %ax\n\t"
		"int $0x16\n\t"
		"xorb %ah, %ah\n\t"
		"cli\n\t"
	);	

	return real_call ( rm_console_getc, NULL, NULL );
}

/**************************************************************************
CONSOLE_ISCHAR - Check for keyboard interrupt
**************************************************************************/
int console_ischar ( void )
{
	RM_FRAGMENT(rm_console_ischar,
		"sti\n\t"
		"movb $1, %ah\n\t"
		"int $0x16\n\t"
		"pushfw\n\t"
		"popw %ax\n\t"
		"cli\n\t"
	);

	return ( ( real_call ( rm_console_ischar, NULL, NULL ) & ZF ) == 0 );
}

/**************************************************************************
GETSHIFT - Get keyboard shift state
**************************************************************************/
int getshift ( void )
{
	RM_FRAGMENT(rm_getshift,
		"sti\n\t"
		"movb $2, %ah\n\t"
		"int $0x16\n\t"
		"andw $0x3, %ax\n\t" 
		"cli\n\t"
	);

	return real_call ( rm_getshift, NULL, NULL );
}

#endif /* PCBIOS */
