#ifndef _GPXE_SEGMENT_H
#define _GPXE_SEGMENT_H

/**
 * @file
 *
 * Executable image segments
 *
 */

#include <gpxe/uaccess.h>

extern int prep_segment ( userptr_t segment, size_t filesz, size_t memsz );

#endif /* _GPXE_SEGMENT_H */
