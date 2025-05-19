#ifndef _IPXE_UHEAP_H
#define _IPXE_UHEAP_H

/** @file
 *
 * External ("user") heap
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef UMALLOC_UHEAP
#define UMALLOC_PREFIX_uheap
#else
#define UMALLOC_PREFIX_uheap __uheap_
#endif

#endif /* _IPXE_UHEAP_H */
