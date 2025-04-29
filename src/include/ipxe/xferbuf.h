#ifndef _IPXE_XFERBUF_H
#define _IPXE_XFERBUF_H

/** @file
 *
 * Data transfer buffer
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/iobuf.h>
#include <ipxe/interface.h>
#include <ipxe/xfer.h>

/** A data transfer buffer */
struct xfer_buffer {
	/** Data */
	void *data;
	/** Size of data */
	size_t len;
	/** Current offset within data */
	size_t pos;
	/** Data transfer buffer operations */
	struct xfer_buffer_operations *op;
};

/** Data transfer buffer operations */
struct xfer_buffer_operations {
	/** Reallocate data buffer
	 *
	 * @v xferbuf		Data transfer buffer
	 * @v len		New length (or zero to free buffer)
	 * @ret rc		Return status code
	 */
	int ( * realloc ) ( struct xfer_buffer *xferbuf, size_t len );
};

extern struct xfer_buffer_operations xferbuf_malloc_operations;
extern struct xfer_buffer_operations xferbuf_umalloc_operations;
extern struct xfer_buffer_operations xferbuf_fixed_operations;
extern struct xfer_buffer_operations xferbuf_void_operations;

/**
 * Initialise malloc()-based data transfer buffer
 *
 * @v xferbuf		Data transfer buffer
 *
 * Data will be automatically allocated using malloc().
 */
static inline __attribute__ (( always_inline )) void
xferbuf_malloc_init ( struct xfer_buffer *xferbuf ) {
	xferbuf->op = &xferbuf_malloc_operations;
}

/**
 * Initialise umalloc()-based data transfer buffer
 *
 * @v xferbuf		Data transfer buffer
 *
 * Data will be automatically allocated using umalloc() (and may
 * therefore alter the system memory map).
 */
static inline __attribute__ (( always_inline )) void
xferbuf_umalloc_init ( struct xfer_buffer *xferbuf ) {
	xferbuf->op = &xferbuf_umalloc_operations;
}

/**
 * Initialise fixed-size data transfer buffer
 *
 * @v xferbuf		Data transfer buffer
 * @v data		Data buffer
 * @v len		Length of data buffer
 *
 * Data will be never be automatically allocated.
 */
static inline __attribute__ (( always_inline )) void
xferbuf_fixed_init ( struct xfer_buffer *xferbuf, void *data, size_t len ) {
	xferbuf->data = data;
	xferbuf->len = len;
	xferbuf->op = &xferbuf_fixed_operations;
}

/**
 * Initialise void data transfer buffer
 *
 * @v xferbuf		Data transfer buffer
 *
 * No data will be allocated, but the length will be recorded.  This
 * can be used to capture xfer_seek() results.
 */
static inline __attribute__ (( always_inline )) void
xferbuf_void_init ( struct xfer_buffer *xferbuf ) {
	xferbuf->op = &xferbuf_void_operations;
}

extern void xferbuf_detach ( struct xfer_buffer *xferbuf );
extern void xferbuf_free ( struct xfer_buffer *xferbuf );
extern int xferbuf_write ( struct xfer_buffer *xferbuf, size_t offset,
			   const void *data, size_t len );
extern int xferbuf_read ( struct xfer_buffer *xferbuf, size_t offset,
			  void *data, size_t len );
extern int xferbuf_deliver ( struct xfer_buffer *xferbuf,
			     struct io_buffer *iobuf,
			     struct xfer_metadata *meta );

extern struct xfer_buffer * xfer_buffer ( struct interface *intf );
#define xfer_buffer_TYPE( object_type ) \
	typeof ( struct xfer_buffer * ( object_type ) )

#endif /* _IPXE_XFERBUF_H */
