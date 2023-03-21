#ifndef _IPXE_EFI_CMDLINE_H
#define _IPXE_EFI_CMDLINE_H

/** @file
 *
 * EFI command line
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <wchar.h>

extern const wchar_t *efi_cmdline;
extern size_t efi_cmdline_len;

#endif /* _IPXE_EFI_CMDLINE_H */
