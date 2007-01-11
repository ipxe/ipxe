#ifndef _GPXE_ELF_H
#define _GPXE_ELF_H

/**
 * @file
 *
 * ELF image format
 *
 */

#include <elf.h>

/** An ELF file */
struct elf {
	/** ELF file image */
	userptr_t image;
	/** Length of ELF file image */
	size_t len;

	/** Entry point */
	physaddr_t entry;
};

extern int elf_load ( struct elf *elf );

#endif /* _GPXE_ELF_H */
