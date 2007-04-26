#ifndef _GPXE_IOBUF_H
#define _GPXE_IOBUF_H

/** @file
 *
 * I/O buffers
 *
 */

#include <stdint.h>
#include <assert.h>
#include <gpxe/list.h>

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
 * @v iob	I/O buffer
 * @v len	Length to reserve
 * @ret data	Pointer to new start of buffer
 */
static inline void * iob_reserve ( struct io_buffer *iob, size_t len ) {
	iob->data += len;
	iob->tail += len;
	assert ( iob->tail <= iob->end );
	return iob->data;
}

/**
 * Add data to start of I/O buffer
 *
 * @v iob	I/O buffer
 * @v len	Length to add
 * @ret data	Pointer to new start of buffer
 */
static inline void * iob_push ( struct io_buffer *iob, size_t len ) {
	iob->data -= len;
	assert ( iob->data >= iob->head );
	return iob->data;
}

/**
 * Remove data from start of I/O buffer
 *
 * @v iob	I/O buffer
 * @v len	Length to remove
 * @ret data	Pointer to new start of buffer
 */
static inline void * iob_pull ( struct io_buffer *iob, size_t len ) {
	iob->data += len;
	assert ( iob->data <= iob->tail );
	return iob->data;
}

/**
 * Add data to end of I/O buffer
 *
 * @v iob	I/O buffer
 * @v len	Length to add
 * @ret data	Pointer to newly added space
 */
static inline void * iob_put ( struct io_buffer *iob, size_t len ) {
	void *old_tail = iob->tail;
	iob->tail += len;
	assert ( iob->tail <= iob->end );
	return old_tail;
}

/**
 * Remove data from end of I/O buffer
 *
 * @v iob	I/O buffer
 * @v len	Length to remove
 */
static inline void iob_unput ( struct io_buffer *iob, size_t len ) {
	iob->tail -= len;
	assert ( iob->tail >= iob->data );
}

/**
 * Empty an I/O buffer
 *
 * @v iob	I/O buffer
 */
static inline void iob_empty ( struct io_buffer *iob ) {
	iob->tail = iob->data;
}

/**
 * Calculate length of data in an I/O buffer
 *
 * @v iob	I/O buffer
 * @ret len	Length of data in buffer
 */
static inline size_t iob_len ( struct io_buffer *iob ) {
	return ( iob->tail - iob->data );
}

/**
 * Calculate available space at start of an I/O buffer
 *
 * @v iob	I/O buffer
 * @ret len	Length of data available at start of buffer
 */
static inline size_t iob_headroom ( struct io_buffer *iob ) {
	return ( iob->data - iob->head );
}

/**
 * Calculate available space at end of an I/O buffer
 *
 * @v iob	I/O buffer
 * @ret len	Length of data available at end of buffer
 */
static inline size_t iob_tailroom ( struct io_buffer *iob ) {
	return ( iob->end - iob->tail );
}

extern struct io_buffer * alloc_iob ( size_t len );
extern void free_iob ( struct io_buffer *iob );
extern void iob_pad ( struct io_buffer *iob, size_t min_len );

#endif /* _GPXE_IOBUF_H */
