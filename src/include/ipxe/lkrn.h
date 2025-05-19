#ifndef _IPXE_LKRN_H
#define _IPXE_LKRN_H

/** @file
 *
 * Linux kernel images
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

/** Kernel image header */
struct lkrn_header {
	/** Executable code */
	uint32_t code[2];
	/** Image load offset */
	uint64_t text_offset;
	/** Image size */
	uint64_t image_size;
	/** Flags */
	uint64_t flags;
	/** Reserved */
	uint8_t reserved_a[24];
	/** Magic */
	uint32_t magic;
	/** Reserved */
	uint8_t reserved_b[4];
} __attribute__ (( packed ));

/** Kernel magic value */
#define LKRN_MAGIC( a, b, c, d ) \
	( ( (a) << 0 ) | ( (b) << 8 ) | ( (c) << 16 ) | ( (d) << 24 ) )

/** Kernel magic value for AArch64 */
#define LKRN_MAGIC_AARCH64 LKRN_MAGIC ( 'A', 'R', 'M', 0x64 )

/** Kernel magic value for RISC-V */
#define LKRN_MAGIC_RISCV LKRN_MAGIC ( 'R', 'S', 'C', 0x05 )

/** Kernel image context */
struct lkrn_context {
	/** Load offset */
	size_t offset;
	/** File size */
	size_t filesz;
	/** Memory size */
	size_t memsz;

	/** Start of RAM */
	physaddr_t ram;
	/** Entry point */
	physaddr_t entry;
	/** Device tree */
	physaddr_t fdt;
};

#include <bits/lkrn.h>

/**
 * Jump to kernel entry point
 *
 * @v entry		Kernel entry point
 * @v fdt		Device tree
 */
void lkrn_jump ( physaddr_t entry, physaddr_t fdt );

#endif /* _IPXE_LKRN_H */
