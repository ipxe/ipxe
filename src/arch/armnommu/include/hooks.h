/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef ETHERBOOT_ARM_HOOKS_H
#define ETHERBOOT_ARM_HOOKS_H

struct Elf_Bhdr;

#define arch_main(data, params) do {} while(0)
//void arch_main(in_call_data_t *data, va_list params);

#define arch_on_exit(status) do {} while(0)
//void arch_on_exit(int status);

#define arch_relocate_to(addr) do {} while(0)
//void arch_relocate_to(unsigned long addr);

#define arch_relocated_from(old_addr) do {} while(0)
//void arch_relocate_from(unsigned long old_addr);

#endif /* ETHERBOOT_ARM_HOOKS_H */
