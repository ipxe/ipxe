#ifndef _IPXE_EFILOONG64_NAP_H
#define _IPXE_EFILOONG64_NAP_H

/** @file
 *
 * EFI CPU sleeping
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef NAP_EFILOONG64
#define NAP_PREFIX_efiloong64
#else
#define NAP_PREFIX_efiloong64 __efiloong64_
#endif

#endif /* _IPXE_EFILOONG64_NAP_H */
