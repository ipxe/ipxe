/* Utility functions to hide Etherboot by manipulating the E820 memory
 * map.  These could go in memsizes.c, but are placed here because not
 * all images will need them.
 */

#include "etherboot.h"
#include "hidemem.h"

#ifdef	CODE16

static int mangling = 0;
static void *mangler = NULL;

#define INSTALLED(x)	( (typeof(&x)) ( (void*)(&x) - (void*)e820mangler \
					 + mangler ) )
#define intercept_int15		INSTALLED(_intercept_int15)
#define intercepted_int15	INSTALLED(_intercepted_int15)
#define hide_memory		INSTALLED(_hide_memory)
#define INT15_VECTOR ( (segoff_t*) ( phys_to_virt( 4 * 0x15 ) ) )

int install_e820mangler ( void *new_mangler ) {
	if ( mangling ) return 0;
	memcpy ( new_mangler, &e820mangler, e820mangler_size );
	mangler = new_mangler;
	return 1;
}

/* Intercept INT15 calls and pass them through the mangler.  The
 * mangler must have been copied to base memory before making this
 * call, and "mangler" must point to the base memory copy, which must
 * be 16-byte aligned.
 */
int hide_etherboot ( void ) {
	if ( mangling ) return 1;
	if ( !mangler ) return 0;

	/* Hook INT15 handler */
	*intercepted_int15 = *INT15_VECTOR;
	(*hide_memory)[0].start = virt_to_phys(_text);
	(*hide_memory)[0].length = _end - _text;
	/* IMPORTANT, possibly even FIXME:
	 *
	 * Etherboot has a tendency to claim a very large area of
	 * memory as possible heap; enough to make it impossible to
	 * load an OS if we hide all of it.  We hide only the portion
	 * that's currently in use.  This means that we MUST NOT
	 * perform further allocations from the heap while the mangler
	 * is active.
	 */
	(*hide_memory)[1].start = heap_ptr;
	(*hide_memory)[1].length = heap_bot - heap_ptr;
	INT15_VECTOR->segment = SEGMENT(mangler);
	INT15_VECTOR->offset = 0;

	mangling = 1;
	return 1;
}

int unhide_etherboot ( void ) {
	if ( !mangling ) return 1;

	/* Restore original INT15 handler
	 */
	if ( VIRTUAL(INT15_VECTOR->segment,INT15_VECTOR->offset) != mangler ) {
		/* Oh dear... */

#ifdef WORK_AROUND_BPBATCH_BUG
		/* BpBatch intercepts INT15, so can't unhook it, and
		 * then proceeds to ignore our PXENV_KEEP_UNDI return
		 * status, which means that it ends up zeroing out the
		 * INT15 handler routine.
		 *
		 * This rather ugly hack involves poking into
		 * BpBatch's code and changing it's stored value for
		 * the "next handler" in the INT15 chain.
		 */
		segoff_t *bp_chain = VIRTUAL ( 0x0060, 0x8254 );

		if ( ( bp_chain->segment == SEGMENT(mangler) ) &&
		     ( bp_chain->offset == 0 ) ) {
			printf ( "\nBPBATCH bug workaround enabled\n" );
			*bp_chain = *intercepted_int15;
		}
#endif /* WORK_AROUND_BPBATCH_BUG */

		return 0;
	}
	*INT15_VECTOR = *intercepted_int15;

	mangling = 0;
	return 1;
}

#endif	/* CODE16 */
