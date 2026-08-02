#ifndef _IPXE_IOBUF_H
#define _IPXE_IOBUF_H

/** @file
 *
 * I/O buffers
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <assert.h>
#include <ipxe/list.h>
#include <ipxe/dma.h>

/**
 * Minimum I/O buffer length and alignment
 *
 * alloc_iob() will round up the allocated length to this size if
 * necessary.  This is used on behalf of hardware that is not capable
 * of auto-padding.
 *
 * This length must be at least as large as the largest cacheline size
 * that we expect to encounter, to allow for platforms where DMA
 * devices are not in the same coherency domain as the CPU cache.
 */
#define IOB_ZLEN 128

/**
 * A persistent I/O buffer
 *
 * This data structure encapsulates a long-lived I/O buffer.  The
 * buffer may be passed between multiple owners, queued for possible
 * retransmission, etc.
 *
 * The datapath model uses a zero-copy fast path for both transmit and
 * receive directions.  Headers and footers are appended/prepended and
 * stripped in situ as the buffer is passed between layers of the
 * network stack.  Adding or stripping a header or footer is merely a
 * pointer update: no existing data is ever moved.
 *
 * The buffer content is delineated by four pointers, which satisfy
 * the invariant:
 *
 *     head <= data <= tail <= end
 *
 * The head and end pointers are set when the buffer is first
 * allocated and are never changed.  The data and tail pointers
 * represent the current data region within the buffer, and are
 * modified by the accessor functions iob_push(), iob_pull(),
 * iob_put(), iob_unput() etc.
 *
 * The current length of the data region may be obtained using
 * iob_len().  The space currently between the head and data pointers
 * is the available headroom and its length may be obtained using
 * iob_headroom().  Similarly, the space currently between the tail
 * and end pointers is the available tailroom and its length may be
 * obtained using iob_tailroom().
 *
 * It is the responsibility of the allocator of the I/O buffer to
 * ensure that sufficient headroom and tailroom exists for all
 * subsequent users of the I/O buffer.  For example: a transmit buffer
 * allocated by the TCP layer must ensure that there is sufficient
 * headroom for the TCP headers, the network-layer (IPv4/IPv6)
 * headers, and the longest possible link-layer header.  The lower
 * layers are permitted to assume that sufficient headroom exists and
 * may call iob_push() to prepend their headers without performing any
 * further checks.
 *
 * On the receive datapath, I/O buffers are typically allocated by the
 * device driver.  Some care must be taken to ensure that received
 * buffers that end up being reflected and transmitted (e.g. responses
 * to ARP requests) contain sufficient headroom.  For most devices,
 * transmit and receive buffers are symmetric and so any receive
 * buffer will always have sufficient headroom for this purpose.
 * Devices that require additional transmit headers (such as the Asix
 * USB NICs) must ensure that additional headroom is allocated in
 * receive buffers to allow for this reflection.
 *
 * Received I/O buffers should always be treated as containing
 * untrusted data.  Device drivers may assume that DMA-capable
 * hardware will not report erroneous lengths (e.g. a received length
 * greater than the original allocation length), but all other
 * consumers must validate the buffer length before accessing its
 * contents or stripping headers or footers.
 *
 * The accessor functions iob_push(), iob_pull(), iob_unput() etc
 * include assertion checks but do not perform any runtime checks that
 * the pointer invariant is maintained.  In particular, using
 * iob_pull() or iob_unput() to strip a header without first using
 * iob_len() to check the available length will result in an invariant
 * violation that causes the iob_len() calculation to underflow and
 * report an extremely large buffer length (which is then likely to
 * cause a false positive for any subsequent buffer length checks).
 */
struct io_buffer {
	/** List of which this buffer is a member
	 *
	 * The list must belong to the current owner of the buffer.
	 * Different owners may maintain different lists (e.g. a
	 * retransmission list for TCP).
	 */
	struct list_head list;

	/** DMA mapping */
	struct dma_mapping map;

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
	return iobuf->data;
}
#define iob_reserve( iobuf, len ) ( {			\
	void *__result;					\
	__result = iob_reserve ( (iobuf), (len) );	\
	assert ( (iobuf)->tail <= (iobuf)->end );	\
	__result; } )

/**
 * Add data to start of I/O buffer
 *
 * @v iobuf	I/O buffer
 * @v len	Length to add
 * @ret data	Pointer to new start of buffer
 */
static inline void * iob_push ( struct io_buffer *iobuf, size_t len ) {
	iobuf->data -= len;
	return iobuf->data;
}
#define iob_push( iobuf, len ) ( {			\
	void *__result;					\
	__result = iob_push ( (iobuf), (len) );		\
	assert ( (iobuf)->data >= (iobuf)->head );	\
	__result; } )

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
#define iob_pull( iobuf, len ) ( {			\
	void *__result;					\
	__result = iob_pull ( (iobuf), (len) );		\
	assert ( (iobuf)->data <= (iobuf)->tail );	\
	__result; } )

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
	return old_tail;
}
#define iob_put( iobuf, len ) ( {			\
	void *__result;					\
	__result = iob_put ( (iobuf), (len) );		\
	assert ( (iobuf)->tail <= (iobuf)->end );	\
	__result; } )

/**
 * Remove data from end of I/O buffer
 *
 * @v iobuf	I/O buffer
 * @v len	Length to remove
 */
static inline void iob_unput ( struct io_buffer *iobuf, size_t len ) {
	iobuf->tail -= len;
}
#define iob_unput( iobuf, len ) do {			\
	iob_unput ( (iobuf), (len) );			\
	assert ( (iobuf)->tail >= (iobuf)->data );	\
	} while ( 0 )

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
 * Create a temporary I/O buffer
 *
 * @v iobuf	I/O buffer
 * @v data	Data buffer
 * @v len	Length of data
 * @v max_len	Length of buffer
 *
 * It is sometimes useful to use the iob_xxx() methods on temporary
 * data buffers.
 */
static inline void iob_populate ( struct io_buffer *iobuf,
				  void *data, size_t len, size_t max_len ) {
	iobuf->head = iobuf->data = data;
	iobuf->tail = ( data + len );
	iobuf->end = ( data + max_len );
}

/**
 * Disown an I/O buffer
 *
 * @v iobuf	I/O buffer
 *
 * There are many functions that take ownership of the I/O buffer they
 * are passed as a parameter.  The caller should not retain a pointer
 * to the I/O buffer.  Use iob_disown() to automatically nullify the
 * caller's pointer, e.g.:
 *
 *     xfer_deliver_iob ( xfer, iob_disown ( iobuf ) );
 *
 * This will ensure that iobuf is set to NULL for any code after the
 * call to xfer_deliver_iob().
 */
#define iob_disown( iobuf ) ( {				\
	struct io_buffer *__iobuf = (iobuf);		\
	(iobuf) = NULL;					\
	__iobuf; } )

/**
 * Map I/O buffer for DMA
 *
 * @v iobuf		I/O buffer
 * @v dma		DMA device
 * @v len		Length to map
 * @v flags		Mapping flags
 * @ret rc		Return status code
 */
static inline __always_inline int iob_map ( struct io_buffer *iobuf,
					    struct dma_device *dma,
					    size_t len, int flags ) {
	return dma_map ( dma, &iobuf->map, iobuf->data, len, flags );
}

/**
 * Map I/O buffer for transmit DMA
 *
 * @v iobuf		I/O buffer
 * @v dma		DMA device
 * @ret rc		Return status code
 */
static inline __always_inline int iob_map_tx ( struct io_buffer *iobuf,
					       struct dma_device *dma ) {
	return iob_map ( iobuf, dma, iob_len ( iobuf ), DMA_TX );
}

/**
 * Map empty I/O buffer for receive DMA
 *
 * @v iobuf		I/O buffer
 * @v dma		DMA device
 * @ret rc		Return status code
 */
static inline __always_inline int iob_map_rx ( struct io_buffer *iobuf,
					       struct dma_device *dma ) {
	assert ( iob_len ( iobuf ) == 0 );
	return iob_map ( iobuf, dma, iob_tailroom ( iobuf ), DMA_RX );
}

/**
 * Get I/O buffer DMA address
 *
 * @v iobuf		I/O buffer
 * @ret addr		DMA address
 */
static inline __always_inline physaddr_t iob_dma ( struct io_buffer *iobuf ) {
	return dma ( &iobuf->map, iobuf->data );
}

/**
 * Unmap I/O buffer for DMA
 *
 * @v iobuf		I/O buffer
 * @v dma		DMA device
 * @ret rc		Return status code
 */
static inline __always_inline void iob_unmap ( struct io_buffer *iobuf ) {
	dma_unmap ( &iobuf->map, iob_len ( iobuf ) );
}

extern struct io_buffer * __malloc alloc_iob_raw ( size_t len, size_t align,
						   size_t offset );
extern struct io_buffer * __malloc alloc_iob ( size_t len );
extern void free_iob ( struct io_buffer *iobuf );
extern struct io_buffer * __malloc alloc_rx_iob ( size_t len,
						  struct dma_device *dma );
extern void free_rx_iob ( struct io_buffer *iobuf );
extern void iob_pad ( struct io_buffer *iobuf, size_t min_len );
extern int iob_ensure_headroom ( struct io_buffer *iobuf, size_t len );
extern struct io_buffer * iob_concatenate ( struct list_head *list );
extern struct io_buffer * iob_split ( struct io_buffer *iobuf, size_t len );

#endif /* _IPXE_IOBUF_H */
