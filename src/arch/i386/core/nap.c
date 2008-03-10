
#include <realmode.h>
#include <bios.h>

/**************************************************************************
 * Save power by halting the CPU until the next interrupt
 **************************************************************************/
void cpu_nap ( void ) {
	__asm__ __volatile__ ( REAL_CODE ( "sti\n\t"
					   "hlt\n\t"
					   "cli\n\t" ) : : );
}
