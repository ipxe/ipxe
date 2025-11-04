/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/io.h>
#include <ipxe/x86_io.h>

/** @file
 *
 * iPXE I/O API for x86
 *
 */

/** Threshold for port I/O-mapped addresses
 *
 * On x86, port I/O instructions (inb/outb/etc) can take only an 8-bit
 * or 16-bit address (in %dx).  All I/O ports must therefore have a
 * value in the first 64kB of the address space.
 *
 * Virtual addresses below 64kB can never be MMIO addresses:
 *
 * - In the UEFI memory model and x86_64 BIOS memory model, virtual
 *   addresses below 64kB are identity-mapped to the corresponding
 *   physical address.  Since the first 64kB of address space is
 *   always RAM, no MMIO device can exist within this region.
 *
 * - In the i386 BIOS memory model, virtual addresses below 64kB cover
 *   the iPXE binary itself (which starts at address zero).  Since the
 *   size of .textdata can never realistically be below 64kB (not
 *   least since the heap alone is 512kB), and since iPXE is placed
 *   into RAM as a contiguous block, no MMIO device can exist within
 *   this region.
 *
 * We therefore know that any (virtual) address returned by ioremap()
 * must be outside the first 64kB of the address space.  We can
 * therefore use this as a threshold to determine whether a given
 * address is a port I/O address or an MMIO address.
 */
#define PIO_THRESHOLD 0x10000

/**
 * Read from I/O-mapped or memory-mapped device
 *
 * @v io_addr		I/O address
 * @ret data		Value read
 */
#define X86_IOREADX( _api_func, _suffix, _type )			      \
static _type x86_ ## _api_func ( volatile _type *io_addr ) {		      \
	if ( ( ( intptr_t ) io_addr ) < PIO_THRESHOLD ) {		      \
		return in ## _suffix ( io_addr );			      \
	} else {							      \
		return read ## _suffix ( io_addr );			      \
	}								      \
}
X86_IOREADX ( ioread8, b, uint8_t );
X86_IOREADX ( ioread16, w, uint16_t );
X86_IOREADX ( ioread32, l, uint32_t );

/**
 * Write to I/O-mapped or memory-mapped device
 *
 * @v data		Value to write
 * @v io_addr		I/O address
 */
#define X86_IOWRITEX( _api_func, _suffix, _type )			      \
static void x86_ ## _api_func ( _type data, volatile _type *io_addr ) {	      \
	if ( ( ( intptr_t ) io_addr ) < PIO_THRESHOLD ) {		      \
		out ## _suffix ( data, io_addr );			      \
	} else {							      \
		write ## _suffix ( data, io_addr );			      \
	}								      \
}
X86_IOWRITEX ( iowrite8, b, uint8_t );
X86_IOWRITEX ( iowrite16, w, uint16_t );
X86_IOWRITEX ( iowrite32, l, uint32_t );

/**
 * Read 64-bit qword from memory-mapped device
 *
 * @v io_addr		I/O address
 * @ret data		Value read
 *
 * This routine uses MMX instructions.
 */
static __unused uint64_t i386_readq ( volatile uint64_t *io_addr ) {
	uint64_t data;
        __asm__ __volatile__ ( "pushl %%edx\n\t"
			       "pushl %%eax\n\t"
			       "movq (%1), %%mm0\n\t"
			       "movq %%mm0, (%%esp)\n\t"
			       "popl %%eax\n\t"
			       "popl %%edx\n\t"
			       "emms\n\t"
                               : "=A" ( data ) : "r" ( io_addr ) );
	return data;
}

/**
 * Write 64-bit qword to memory-mapped device
 *
 * @v data		Value to write
 * @v io_addr		I/O address
 *
 * This routine uses MMX instructions.
 */
static __unused void i386_writeq ( uint64_t data, volatile uint64_t *io_addr ) {
	__asm__ __volatile__ ( "pushl %%edx\n\t"
			       "pushl %%eax\n\t"
			       "movq (%%esp), %%mm0\n\t"
			       "movq %%mm0, (%1)\n\t"
			       "popl %%eax\n\t"
			       "popl %%edx\n\t"
			       "emms\n\t"
			       : : "A" ( data ), "r" ( io_addr ) );
}

PROVIDE_IOAPI_INLINE ( x86, phys_to_bus );
PROVIDE_IOAPI_INLINE ( x86, bus_to_phys );
PROVIDE_IOAPI_INLINE ( x86, readb );
PROVIDE_IOAPI_INLINE ( x86, readw );
PROVIDE_IOAPI_INLINE ( x86, readl );
PROVIDE_IOAPI_INLINE ( x86, writeb );
PROVIDE_IOAPI_INLINE ( x86, writew );
PROVIDE_IOAPI_INLINE ( x86, writel );
PROVIDE_IOAPI_INLINE ( x86, inb );
PROVIDE_IOAPI_INLINE ( x86, inw );
PROVIDE_IOAPI_INLINE ( x86, inl );
PROVIDE_IOAPI_INLINE ( x86, outb );
PROVIDE_IOAPI_INLINE ( x86, outw );
PROVIDE_IOAPI_INLINE ( x86, outl );
PROVIDE_IOAPI_INLINE ( x86, insb );
PROVIDE_IOAPI_INLINE ( x86, insw );
PROVIDE_IOAPI_INLINE ( x86, insl );
PROVIDE_IOAPI_INLINE ( x86, outsb );
PROVIDE_IOAPI_INLINE ( x86, outsw );
PROVIDE_IOAPI_INLINE ( x86, outsl );
PROVIDE_IOAPI_INLINE ( x86, iodelay );
PROVIDE_IOAPI_INLINE ( x86, mb );
#ifdef __x86_64__
PROVIDE_IOAPI_INLINE ( x86, readq );
PROVIDE_IOAPI_INLINE ( x86, writeq );
#else
PROVIDE_IOAPI ( x86, readq, i386_readq );
PROVIDE_IOAPI ( x86, writeq, i386_writeq );
#endif
PROVIDE_IOAPI ( x86, ioread8, x86_ioread8 );
PROVIDE_IOAPI ( x86, ioread16, x86_ioread16 );
PROVIDE_IOAPI ( x86, ioread32, x86_ioread32 );
PROVIDE_IOAPI ( x86, iowrite8, x86_iowrite8 );
PROVIDE_IOAPI ( x86, iowrite16, x86_iowrite16 );
PROVIDE_IOAPI ( x86, iowrite32, x86_iowrite32 );
