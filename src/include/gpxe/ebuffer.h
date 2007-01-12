#ifndef _GPXE_EBUFFER_H
#define _GPXE_EBUFFER_H

/**
 * @file
 *
 * Automatically expanding buffers
 *
 */

struct buffer;

extern int ebuffer_alloc ( struct buffer *buffer, size_t len );

#endif /* _GPXE_EBUFFER_H */
