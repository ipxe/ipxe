#ifndef _IPXE_EFI_MP_H
#define _IPXE_EFI_MP_H

/** @file
 *
 * EFI multiprocessor API implementation
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef MPAPI_EFI
#define MPAPI_PREFIX_efi
#else
#define MPAPI_PREFIX_efi __efi_
#endif

/**
 * Calculate address as seen by a multiprocessor function
 *
 * @v address		Address in boot processor address space
 * @ret address		Address in application processor address space
 */
static inline __attribute__ (( always_inline )) mp_addr_t
MPAPI_INLINE ( efi, mp_address ) ( void *address ) {

	return ( ( mp_addr_t ) address );
}

#endif /* _IPXE_EFI_MP_H */
