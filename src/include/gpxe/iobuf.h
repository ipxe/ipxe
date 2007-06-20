#ifndef _GPXE_IOBUF_H
#define _GPXE_IOBUF_H

/** @file
 *
 * I/O buffers
 *
 */

#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <gpxe/list.h>

/**
 * I/O buffer alignment
 *
 * I/O buffers allocated via alloc_iob() are guaranteed to be
 * physically aligned to this boundary.  Some cards cannot DMA across
 * a 4kB boundary.  With a standard Ethernet MTU, aligning to a 2kB
 * boundary is sufficient to guarantee no 4kB boundary crossings.  For
 * a jumbo Ethernet MTU, a packet may be larger than 4kB anyway.
 */
#define IOB_ALIGN 2048

/**
 * Minimum I/O buffer length
 *
 * alloc_iob() will round up the allocated length to this size if
 * necessary.  This is used on behalf of hardware that is not capable
 * of auto-padding.
 */
#define IOB_ZLEN 64

/**
 * A persistent I/O buffer
 *
 * This data structure encapsulates a long-lived I/O buffer.  The
 * buffer may be passed between multiple owners, queued for possible
 * retransmission, etc.
 */
struct io_buffer {
	/** List of which this buffer is a member
	 *
	 * The list must belong to the current owner of the buffer.
	 * Different owners may maintain different lists (e.g. a
	 * retransmission list for TCP).
	 */
	struct list_head list;

	/** Start of the buffer */
	void *head;
	/** Start of data */
	void *data;
	/** End of data */
	void *tail;
	/** End of the buffer */
        void *end;
};

/**
 * Reserve space at start of I/O buffer
 *
 * @v iobuf	I/O buffer
 * @v len	Length to reserve
 * @ret data	Pointer to new start of buffer
 */
static inline void * iob_reserve ( struct io_buffer *iobuf, size_t len ) {
	iobuf->data += len;
	iobuf->tail += len;
	assert ( iobuf->tail <= iobuf->end );
	return iobuf->data;
}

/**
 * Add data to start of I/O buffer
 *
 * @v iobuf	I/O buffer
 * @v len	Length to add
 * @ret data	Pointer to new start of buffer
 */
static inline void * iob_push ( struct io_buffer *iobuf, size_t len ) {
	iobuf->data -= len;
	assert ( iobuf->data >= iobuf->head );
	return iobuf->data;
}

/**
 * Remove data from start of I/O buffer
 *
 * @v iobuf	I/O buffer
 * @v len	Length to remove
 * @ret data	Pointer to new start of buffer
 */
static inline void * iob_pull ( struct io_buffer *iobuf, size_t len ) {
	iobuf->data += len;
	assert ( iobuf->data <= iobuf->tail );
	return iobuf->data;
}

/**
 * Add data to end of I/O buffer
 *
 * @v iobuf	I/O buffer
 * @v len	Length to add
 * @ret data	Pointer to newly added space
 */
static inline void * iob_put ( struct io_buffer *iobuf, size_t len ) {
	void *old_tail = iobuf->tail;
	iobuf->tail += len;
	assert ( iobuf->tail <= iobuf->end );
	return old_tail;
}

/**
 * Remove data from end of I/O buffer
 *
 * @v iobuf	I/O buffer
 * @v len	Length to remove
 */
static inline void iob_unput ( struct io_buffer *iobuf, size_t len ) {
	iobuf->tail -= len;
	assert ( iobuf->tail >= iobuf->data );
}

/**
 * Empty an I/O buffer
 *
 * @v iobuf	I/O buffer
 */
static inline void iob_empty ( struct io_buffer *iobuf ) {
	iobuf->tail = iobuf->data;
}

/**
 * Calculate length of data in an I/O buffer
 *
 * @v iobuf	I/O buffer
 * @ret len	Length of data in buffer
 */
static inline size_t iob_len ( struct io_buffer *iobuf ) {
	return ( iobuf->tail - iobuf->data );
}

/**
 * Calculate available space at start of an I/O buffer
 *
 * @v iobuf	I/O buffer
 * @ret len	Length of data available at start of buffer
 */
static inline size_t iob_headroom ( struct io_buffer *iobuf ) {
	return ( iobuf->data - iobuf->head );
}

/**
 * Calculate available space at end of an I/O buffer
 *
 * @v iobuf	I/O buffer
 * @ret len	Length of data available at end of buffer
 */
static inline size_t iob_tailroom ( struct io_buffer *iobuf ) {
	return ( iobuf->end - iobuf->tail );
}

/**
 * Ensure I/O buffer has sufficient headroom
 *
 * @v iobuf	I/O buffer
 * @v len	Required headroom
 *
 * This function currently only checks for the required headroom; it
 * does not reallocate the I/O buffer if required.  If we ever have a
 * code path that requires this functionality, it's a fairly trivial
 * change to make.
 */
static inline __attribute__ (( always_inline )) int
iob_ensure_headroom ( struct io_buffer *iobuf, size_t len ) {
	if ( iob_headroom ( iobuf ) >= len )
		return 0;
	return -ENOBUFS;
}

extern struct io_buffer * alloc_iob ( size_t len );
extern void free_iob ( struct io_buffer *iobuf );
extern void iob_pad ( struct io_buffer *iobuf, size_t min_len );

#endif /* _GPXE_IOBUF_H */
