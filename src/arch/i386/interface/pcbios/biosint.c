#include <errno.h>
#include <realmode.h>
#include <biosint.h>

/**
 * @file BIOS interrupts
 *
 */

/**
 * Hooked interrupt count
 *
 * At exit, after unhooking all possible interrupts, this counter
 * should be examined.  If it is non-zero, it means that we failed to
 * unhook at least one interrupt vector, and so must not free up the
 * memory we are using.  (Note that this also implies that we should
 * re-hook INT 15 in order to hide ourselves from the memory map).
 */
int hooked_bios_interrupts = 0;

/**
 * Hook INT vector
 *
 * @v interrupt		INT number
 * @v handler		Offset within .text16 to interrupt handler
 * @v chain_vector	Vector for chaining to previous handler
 *
 * Hooks in an i386 INT handler.  The handler itself must reside
 * within the .text16 segment.  @c chain_vector will be filled in with
 * the address of the previously-installed handler for this interrupt;
 * the handler should probably exit by ljmping via this vector.
 */
void hook_bios_interrupt ( unsigned int interrupt, unsigned int handler,
			   struct segoff *chain_vector ) {
	struct segoff vector = {
		.segment = rm_cs,
		.offset = handler,
	};

	if ( ( chain_vector->segment != 0 ) ||
	     ( chain_vector->offset != 0 ) ) {
		/* Already hooked; do nothing */
		return;
	}
	copy_from_real ( chain_vector, 0, ( interrupt * 4 ),
			 sizeof ( *chain_vector ) );
	copy_to_real ( 0, ( interrupt * 4 ), &vector, sizeof ( vector ) );
	hooked_bios_interrupts++;
}

/**
 * Unhook INT vector
 *
 * @v interrupt		INT number
 * @v handler		Offset within .text16 to interrupt handler
 * @v chain_vector	Vector containing address of previous handler
 *
 * Unhooks an i386 interrupt handler hooked by hook_i386_vector().
 * Note that this operation may fail, if some external code has hooked
 * the vector since we hooked in our handler.  If it fails, it means
 * that it is not possible to unhook our handler, and we must leave it
 * (and its chaining vector) resident in memory.
 */
int unhook_bios_interrupt ( unsigned int interrupt, unsigned int handler,
			    struct segoff *chain_vector ) {
	struct segoff vector;

	copy_from_real ( &vector, 0, ( interrupt * 4 ), sizeof ( vector ) );
	if ( ( vector.segment != rm_cs ) || ( vector.offset != handler ) )
		return -EBUSY;
	copy_to_real ( 0, ( interrupt * 4 ), chain_vector,
		       sizeof ( *chain_vector ) );
	chain_vector->segment = 0;
	chain_vector->offset = 0;
	hooked_bios_interrupts--;
	return 0;
}
