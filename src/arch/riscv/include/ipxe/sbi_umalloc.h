#ifndef _IPXE_SBI_UMALLOC_H
#define _IPXE_SBI_UMALLOC_H

/** @file
 *
 * External memory allocation
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef UMALLOC_SBI
#define UMALLOC_PREFIX_sbi
#else
#define UMALLOC_PREFIX_sbi __sbi_
#endif

#endif /* _IPXE_SBI_UMALLOC_H */
