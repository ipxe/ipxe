#ifndef _IPXE_FDTMEM_H
#define _IPXE_FDTMEM_H

/** @file
 *
 * Flattened Device Tree memory map
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/fdt.h>

extern physaddr_t fdtmem_relocate ( struct fdt_header *hdr, size_t limit );
extern int fdtmem_register ( struct fdt_header *hdr, size_t limit );

#endif /* _IPXE_FDTMEM_H */
