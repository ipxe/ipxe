#ifndef _IPXE_BIOS_MP_H
#define _IPXE_BIOS_MP_H

/** @file
 *
 * BIOS multiprocessor API implementation
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/io.h>

#ifdef MPAPI_PCBIOS
#define MPAPI_PREFIX_pcbios
#else
#define MPAPI_PREFIX_pcbios __pcbios_
#endif

/**
 * Calculate address as seen by a multiprocessor function
 *
 * @v address		Address in boot processor address space
 * @ret address		Address in application processor address space
 */
static inline __attribute__ (( always_inline )) mp_addr_t
MPAPI_INLINE ( pcbios, mp_address ) ( void *address ) {

	return virt_to_phys ( address );
}

#endif /* _IPXE_BIOS_MP_H */
