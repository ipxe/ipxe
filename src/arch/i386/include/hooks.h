#ifndef ETHERBOOT_I386_HOOKS_H
#define ETHERBOOT_I386_HOOKS_H

void arch_main(in_call_data_t *data, va_list params);
void arch_on_exit(int status);
#define arch_relocate_to(addr)
void arch_relocated_from ( uint32_t old_addr );

#endif /* ETHERBOOT_I386_HOOKS_H */
