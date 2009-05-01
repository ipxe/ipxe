#ifndef _GPXE_FILTER_H
#define _GPXE_FILTER_H

/** @file
 *
 * Data transfer filters
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stddef.h>
#include <gpxe/xfer.h>

/**
 * Half of a data transfer filter
 *
 * Embed two of these structures within a structure implementing a
 * data transfer filter, and intialise with filter_init().  You can
 * then use the filter_xxx() methods as the data transfer interface
 * methods as required.
 */
struct xfer_filter_half {
	/** Data transfer interface */
	struct xfer_interface xfer;
	/** Other half of the data transfer filter */
	struct xfer_filter_half *other;
};

/**
 * Get data transfer interface for the other half of a data transfer filter
 *
 * @v xfer		Data transfer interface
 * @ret other		Other half's data transfer interface
 */
static inline __attribute__ (( always_inline )) struct xfer_interface *
filter_other_half ( struct xfer_interface *xfer ) {
	struct xfer_filter_half *half = 
		container_of ( xfer, struct xfer_filter_half, xfer );
	return &half->other->xfer;
}

extern void filter_close ( struct xfer_interface *xfer, int rc );
extern int filter_vredirect ( struct xfer_interface *xfer, int type,
			      va_list args );
extern size_t filter_window ( struct xfer_interface *xfer );
extern struct io_buffer * filter_alloc_iob ( struct xfer_interface *xfer,
					     size_t len );
extern int filter_deliver_iob ( struct xfer_interface *xfer,
				struct io_buffer *iobuf,
				struct xfer_metadata *meta );
extern int filter_deliver_raw ( struct xfer_interface *xfer, const void *data,
				size_t len );

/**
 * Initialise a data transfer filter
 *
 * @v left		"Left" half of the filter
 * @v left_op		Data transfer interface operations for "left" half
 * @v right		"Right" half of the filter
 * @v right_op		Data transfer interface operations for "right" half
 * @v refcnt		Containing object reference counter, or NULL
 */
static inline void filter_init ( struct xfer_filter_half *left,
				 struct xfer_interface_operations *left_op,
				 struct xfer_filter_half *right,
				 struct xfer_interface_operations *right_op,
				 struct refcnt *refcnt ) {
	xfer_init ( &left->xfer, left_op, refcnt );
	xfer_init ( &right->xfer, right_op, refcnt );
	left->other = right;
	right->other = left;
}

#endif /* _GPXE_FILTER_H */
