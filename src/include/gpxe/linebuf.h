#ifndef _GPXE_LINEBUF_H
#define _GPXE_LINEBUF_H

/** @file
 *
 * Line buffering
 *
 */

#include <stdint.h>
#include <stddef.h>

/** A line buffer */
struct line_buffer {
	/** Current data in the buffer */
	char *data;
	/** Length of current data */
	size_t len;
	/** Bitmask of terminating characters to skip over */
	unsigned int skip_terminators;
};

/**
 * Retrieve buffered-up line
 *
 * @v linebuf		Line buffer
 * @ret line		Buffered line, or NULL if no line present
 */
static inline char * buffered_line ( struct line_buffer *linebuf ) {
	return linebuf->data;
}

extern int line_buffer ( struct line_buffer *linebuf, const char *data,
			 size_t len );
extern void empty_line_buffer ( struct line_buffer *linebuf );

#endif /* _GPXE_LINEBUF_H */
