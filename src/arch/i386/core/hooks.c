#include "etherboot.h"
#include "callbacks.h"
#include <stdarg.h>

void arch_main ( in_call_data_t *data __unused, va_list params __unused )
{
#ifdef PCBIOS
	/* Deallocate base memory used for the prefix, if applicable
	 */
	forget_prefix_base_memory();
#endif

}

void arch_relocated_from (unsigned long old_addr )
{

#ifdef PCBIOS
	/* Deallocate base memory used for the Etherboot runtime,
	 * if applicable
	 */
	forget_runtime_base_memory( old_addr );
#endif

}

void arch_on_exit ( int exit_status __unused ) 
{
#ifdef PCBIOS
	/* Deallocate the real-mode stack now.  We will reallocate
	 * the stack if are going to use it after this point.
	 */
	forget_real_mode_stack();
#endif
}
