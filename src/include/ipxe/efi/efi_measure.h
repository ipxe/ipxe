#ifndef _IPXE_EFI_MEASURE_H
#define _IPXE_EFI_MEASURE_H

/*
 * Copyright C 2020, Oracle and/or its affiliates.
 * Licensed under the GPL v2 only.
 */

FILE_LICENCE ( GPL2_ONLY );

/** @file
 *
 * EFI measure
 *
 */

#ifdef MEASURE_EFI
#define MEASURE_PREFIX_efi
#else
#define MEASURE_PREFIX_efi __efi_
#endif

#endif /* _IPXE_EFI_MEASURE_H */
