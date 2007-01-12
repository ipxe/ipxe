#ifndef _GPXE_EMALLOC_H
#define _GPXE_EMALLOC_H

/**
 * @file
 *
 * External memory allocation
 *
 */

#include <gpxe/uaccess.h>

extern userptr_t emalloc ( size_t size );
extern userptr_t erealloc ( userptr_t ptr, size_t new_size );
extern void efree ( userptr_t ptr );

#endif /* _GPXE_EMALLOC_H */
