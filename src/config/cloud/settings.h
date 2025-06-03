/* It can often be useful to know the CPU on which a cloud instance is
 * running (e.g. to isolate problems with Azure AMD instances).
 */
#if defined ( __i386__ ) || defined ( __x86_64__ )
#define CPUID_SETTINGS
#endif
