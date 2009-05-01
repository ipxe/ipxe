#ifndef _GPXE_ELF_H
#define _GPXE_ELF_H

/**
 * @file
 *
 * ELF image format
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <elf.h>

extern int elf_load ( struct image *image );

#endif /* _GPXE_ELF_H */
