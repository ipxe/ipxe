/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef ETHERBOOT_IO_H
#define ETHERBOOT_IO_H

#define virt_to_phys(vaddr)	((unsigned long) (vaddr))
#define phys_to_virt(vaddr)	((void *) (vaddr))

#define virt_to_bus virt_to_phys
#define bus_to_virt phys_to_virt

#define iounmap(addr)				((void)0)
#define ioremap(physaddr, size)			(physaddr)

extern unsigned char inb (unsigned long int port);
extern unsigned short int inw (unsigned long int port);
extern unsigned long int inl (unsigned long int port);
extern void outb (unsigned char value, unsigned long int port);
extern void outw (unsigned short value, unsigned long int port);
extern void outl (unsigned long value, unsigned long int port);

#endif /* ETHERBOOT_IO_H */
