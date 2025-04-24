#ifndef _IPXE_SEGMENT_H
#define _IPXE_SEGMENT_H

/**
 * @file
 *
 * Executable image segments
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

extern int prep_segment ( void *segment, size_t filesz, size_t memsz );

#endif /* _IPXE_SEGMENT_H */
