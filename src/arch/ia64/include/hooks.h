#ifndef ETHERBOOT_IA64_HOOKS_H
#define ETHERBOOT_IA64_HOOKS_H

#include <stdarg.h>

void arch_main(in_call_data_t *data, va_list params);
void arch_on_exit(int status);
void arch_relocate_to(unsigned long addr);
#define arch_relocated_from(old_addr) do {} while(0)


#endif /* ETHERBOOT_IA64_HOOKS_H */
