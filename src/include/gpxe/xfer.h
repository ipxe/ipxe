#ifndef _GPXE_XFER_H
#define _GPXE_XFER_H

/** @file
 *
 * Data transfer interfaces
 *
 */

#include <stddef.h>
#include <stdarg.h>
#include <gpxe/interface.h>
#include <gpxe/iobuf.h>

struct xfer_interface;
struct xfer_metadata;

/** Data transfer interface operations */
struct xfer_interface_operations {
	/** Close interface
	 *
	 * @v xfer		Data transfer interface
	 * @v rc		Reason for close
	 */
	void ( * close ) ( struct xfer_interface *xfer, int rc );
	/** Redirect to new location
	 *
	 * @v xfer		Data transfer interface
	 * @v type		New location type
	 * @v args		Remaining arguments depend upon location type
	 * @ret rc		Return status code
	 */
	int ( * vredirect ) ( struct xfer_interface *xfer, int type,
			      va_list args );
	/** Request data
	 *
	 * @v xfer		Data transfer interface
	 * @v offset		Offset to new position
	 * @v whence		Basis for new position
	 * @v len		Length of requested data
	 * @ret rc		Return status code
	 */
	int ( * request ) ( struct xfer_interface *xfer, off_t offset,
			    int whence, size_t len );
	/** Seek to position
	 *
	 * @v xfer		Data transfer interface
	 * @v offset		Offset to new position
	 * @v whence		Basis for new position
	 * @ret rc		Return status code
	 *
	 * @c whence must be one of @c SEEK_SET or @c SEEK_CUR.  A
	 * successful return indicates that the interface is ready to
	 * immediately accept datagrams; return -EAGAIN if this is not
	 * the case.
	 */
	int ( * seek ) ( struct xfer_interface *xfer, off_t offset,
			 int whence );
	/** Allocate I/O buffer
	 *
	 * @v xfer		Data transfer interface
	 * @v len		I/O buffer payload length
	 * @ret iobuf		I/O buffer
	 */
	struct io_buffer * ( * alloc_iob ) ( struct xfer_interface *xfer,
					     size_t len );
	/** Deliver datagram as I/O buffer with metadata
	 *
	 * @v xfer		Data transfer interface
	 * @v iobuf		Datagram I/O buffer
	 * @v meta		Data transfer metadata, or NULL
	 * @ret rc		Return status code
	 *
	 * A data transfer interface that wishes to support only raw
	 * data delivery should set this method to
	 * xfer_deliver_as_raw().
	 *
	 * Interfaces may not temporarily refuse to accept data by
	 * returning -EAGAIN; such a response may be treated as a
	 * fatal error.
	 */
	int ( * deliver_iob ) ( struct xfer_interface *xfer,
				struct io_buffer *iobuf,
				struct xfer_metadata *meta );
	/** Deliver datagram as raw data
	 *
	 * @v xfer		Data transfer interface
	 * @v data		Data buffer
	 * @v len		Length of data buffer
	 * @ret rc		Return status code
	 *
	 * A data transfer interface that wishes to support only I/O
	 * buffer delivery should set this method to
	 * xfer_deliver_as_iob().
	 *
	 * Interfaces may not temporarily refuse to accept data by
	 * returning -EAGAIN; such a response may be treated as a
	 * fatal error.
	 */
	int ( * deliver_raw ) ( struct xfer_interface *xfer,
				const void *data, size_t len );
};

/** A data transfer interface */
struct xfer_interface {
	/** Generic object communication interface */
	struct interface intf;
	/** Operations for received messages */
	struct xfer_interface_operations *op;
};

/** Data transfer metadata */
struct xfer_metadata {
	/** Source socket address, or NULL */
	struct sockaddr *src;
	/** Destination socket address, or NULL */
	struct sockaddr *dest;
	/** Network device, or NULL */
	struct net_device *netdev;
};

/** Basis positions for seek() events */
enum seek_whence {
	SEEK_SET = 0,
	SEEK_CUR,
};

/**
 * Describe seek basis
 *
 * @v whence		Basis for new position
 */
static inline __attribute__ (( always_inline )) const char *
whence_text ( int whence ) {
	switch ( whence ) {
	case SEEK_SET:	return "SET";
	case SEEK_CUR:	return "CUR";
	default:	return "INVALID";
	}
}

extern struct xfer_interface null_xfer;
extern struct xfer_interface_operations null_xfer_ops;

extern void xfer_close ( struct xfer_interface *xfer, int rc );
extern int xfer_vredirect ( struct xfer_interface *xfer, int type,
			    va_list args );
extern int xfer_redirect ( struct xfer_interface *xfer, int type, ... );
extern int xfer_request ( struct xfer_interface *xfer, off_t offset,
			  int whence, size_t len );
extern int xfer_seek ( struct xfer_interface *xfer, off_t offset, int whence );
extern int xfer_ready ( struct xfer_interface *xfer );
extern struct io_buffer * xfer_alloc_iob ( struct xfer_interface *xfer,
					   size_t len );
extern int xfer_deliver_iob ( struct xfer_interface *xfer,
			      struct io_buffer *iobuf );
extern int xfer_deliver_iob_meta ( struct xfer_interface *xfer,
				   struct io_buffer *iobuf,
				   struct xfer_metadata *meta );
extern int xfer_deliver_raw ( struct xfer_interface *xfer,
			      const void *data, size_t len );
extern int xfer_vprintf ( struct xfer_interface *xfer,
			  const char *format, va_list args );
extern int xfer_printf ( struct xfer_interface *xfer,
			 const char *format, ... );

extern void ignore_xfer_close ( struct xfer_interface *xfer, int rc );
extern int ignore_xfer_vredirect ( struct xfer_interface *xfer,
				   int type, va_list args );
extern int ignore_xfer_request ( struct xfer_interface *xfer, off_t offset,
				 int whence, size_t len );
extern int ignore_xfer_seek ( struct xfer_interface *xfer, off_t offset,
			      int whence );
extern struct io_buffer * default_xfer_alloc_iob ( struct xfer_interface *xfer,
						   size_t len );
extern int xfer_deliver_as_raw ( struct xfer_interface *xfer,
				 struct io_buffer *iobuf,
				 struct xfer_metadata *meta );
extern int xfer_deliver_as_iob ( struct xfer_interface *xfer,
				 const void *data, size_t len );
extern int ignore_xfer_deliver_raw ( struct xfer_interface *xfer,
				     const void *data __unused, size_t len );

/**
 * Initialise a data transfer interface
 *
 * @v xfer		Data transfer interface
 * @v op		Data transfer interface operations
 * @v refcnt		Containing object reference counter, or NULL
 */
static inline void xfer_init ( struct xfer_interface *xfer,
			       struct xfer_interface_operations *op,
			       struct refcnt *refcnt ) {
	xfer->intf.dest = &null_xfer.intf;
	xfer->intf.refcnt = refcnt;
	xfer->op = op;
}

/**
 * Get data transfer interface from generic object communication interface
 *
 * @v intf		Generic object communication interface
 * @ret xfer		Data transfer interface
 */
static inline __attribute__ (( always_inline )) struct xfer_interface *
intf_to_xfer ( struct interface *intf ) {
	return container_of ( intf, struct xfer_interface, intf );
}

/**
 * Get reference to destination data transfer interface
 *
 * @v xfer		Data transfer interface
 * @ret dest		Destination interface
 */
static inline __attribute__ (( always_inline )) struct xfer_interface *
xfer_get_dest ( struct xfer_interface *xfer ) {
	return intf_to_xfer ( intf_get ( xfer->intf.dest ) );
}

/**
 * Drop reference to data transfer interface
 *
 * @v xfer		Data transfer interface
 */
static inline __attribute__ (( always_inline )) void
xfer_put ( struct xfer_interface *xfer ) {
	intf_put ( &xfer->intf );
}

/**
 * Plug a data transfer interface into a new destination interface
 *
 * @v xfer		Data transfer interface
 * @v dest		New destination interface
 */
static inline __attribute__ (( always_inline )) void
xfer_plug ( struct xfer_interface *xfer, struct xfer_interface *dest ) {
	plug ( &xfer->intf, &dest->intf );
}

/**
 * Plug two data transfer interfaces together
 *
 * @v a			Data transfer interface A
 * @v b			Data transfer interface B
 */
static inline __attribute__ (( always_inline )) void
xfer_plug_plug ( struct xfer_interface *a, struct xfer_interface *b ) {
	plug_plug ( &a->intf, &b->intf );
}

/**
 * Unplug a data transfer interface
 *
 * @v xfer		Data transfer interface
 */
static inline __attribute__ (( always_inline )) void
xfer_unplug ( struct xfer_interface *xfer ) {
	plug ( &xfer->intf, &null_xfer.intf );
}

/**
 * Stop using a data transfer interface
 *
 * @v xfer		Data transfer interface
 *
 * After calling this method, no further messages will be received via
 * the interface.
 */
static inline void xfer_nullify ( struct xfer_interface *xfer ) {
	xfer->op = &null_xfer_ops;
};

#endif /* _GPXE_XFER_H */
