/*
 * EFI header files rely on having the CPU architecture directory
 * present in the search path in order to pick up ProcessorBind.h.  We
 * use this header file as a quick indirection layer.
 *  - mcb30
 */

#if __i386__
#include <gpxe/efi/Ia32/ProcessorBind.h>
#endif

#if __x86_64__
#include <gpxe/efi/X64/ProcessorBind.h>
#endif
