/* Callout/callback interface for Etherboot
 *
 * This file provides the mechanisms for making calls from Etherboot
 * to external programs and vice-versa.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 */

#include "etherboot.h"
#include "callbacks.h"
#include "realmode.h"
#include "segoff.h"
#include <stdarg.h>

/* Maximum amount of stack data that prefix may request to be passed
 * to its exit routine
 */
#define MAX_PREFIX_STACK_DATA 16

/* Prefix exit routine is defined in prefix object */
extern void prefix_exit ( void );
extern void prefix_exit_end ( void );

/*****************************************************************************
 *
 * IN_CALL INTERFACE
 *
 *****************************************************************************
 */

/* in_call(): entry point for calls in to Etherboot from external code. 
 *
 * Parameters: some set up by assembly code _in_call(), others as
 * passed from external code.
 */
uint32_t i386_in_call ( va_list ap, i386_pm_in_call_data_t pm_data,
			uint32_t opcode ) {
	uint32_t ret;
	i386_rm_in_call_data_t rm_data;
	in_call_data_t in_call_data = { &pm_data, NULL };
	struct {
		int data[MAX_PREFIX_STACK_DATA/4];
	} in_stack;

	/* Fill out rm_data if we were called from real mode */
	if ( opcode & EB_CALL_FROM_REAL_MODE ) {
		in_call_data.rm = &rm_data;
		rm_data = va_arg ( ap, typeof(rm_data) );
		/* Null return address indicates to use the special
		 * prefix exit mechanism, and that there are
		 * parameters on the stack that the prefix wants
		 * handed to its exit routine.
		 */
		if ( rm_data.ret_addr.offset == 0 ) {
			int n = va_arg ( ap, int ) / 4;
			int i;
			for ( i = 0; i < n; i++ ) {
				in_stack.data[i] = va_arg ( ap, int );
			}
		}
	}
	
	/* Hand off to main in_call() routine */
	ret = in_call ( &in_call_data, opcode, ap );

	/* If real-mode return address is null, it means that we
	 * should exit via the prefix's exit path, which is part of
	 * our image.  (This arrangement is necessary since the prefix
	 * code itself may have been vapourised by the time we want to
	 * return.)
	 */
	if ( ( opcode & EB_CALL_FROM_REAL_MODE ) &&
	     ( rm_data.ret_addr.offset == 0 ) ) {
		real_call ( prefix_exit, &in_stack, NULL );
		/* Should never return */
	}
		
	return ret;
}

#ifdef	CODE16

/* install_rm_callback_interface(): install real-mode callback
 * interface at specified address.
 *
 * Real-mode code may then call to this address (or lcall to this
 * address plus RM_IN_CALL_FAR) in order to make an in_call() to
 * Etherboot.
 *
 * Returns the size of the installed code, or 0 if the code could not
 * be installed.
 */
int install_rm_callback_interface ( void *address, size_t available ) {
	if ( available &&
	     ( available < rm_callback_interface_size ) ) return 0;

	/* Inform RM code where to find Etherboot */
	rm_etherboot_location = virt_to_phys(_text);

	/* Install callback interface */
	memcpy ( address, &rm_callback_interface,
		 rm_callback_interface_size );

	return rm_callback_interface_size;
}

#endif	/* CODE16 */
