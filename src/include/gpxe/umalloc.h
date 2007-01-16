#ifndef _GPXE_UMALLOC_H
#define _GPXE_UMALLOC_H

/**
 * @file
 *
 * User memory allocation
 *
 */

#include <gpxe/uaccess.h>

extern userptr_t umalloc ( size_t size );
extern userptr_t urealloc ( userptr_t ptr, size_t new_size );
extern void ufree ( userptr_t ptr );

#endif /* _GPXE_UMALLOC_H */
