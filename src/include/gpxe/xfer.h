#ifndef _GPXE_XFER_H
#define _GPXE_XFER_H

/** @file
 *
 * Data transfer interfaces
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

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
	/** Check flow control window
	 *
	 * @v xfer		Data transfer interface
	 * @ret len		Length of window
	 *
	 * Flow control is regarded as advisory but not mandatory.
	 * Users who have control over their own rate of data
	 * generation should perform a flow control check before
	 * generating new data.  Users who have no control (such as
	 * NIC drivers or filter layers) are not obliged to check.
	 *
	 * Data transfer interfaces must be prepared to accept
	 * datagrams even if they are advertising a window of zero
	 * bytes.
	 */
	size_t ( * window ) ( struct xfer_interface *xfer );
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
	 * @v meta		Data transfer metadata
	 * @ret rc		Return status code
	 *
	 * A data transfer interface that wishes to support only raw
	 * data delivery should set this method to
	 * xfer_deliver_as_raw().
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

/** Basis positions for seek() events */
enum seek_whence {
	SEEK_CUR = 0,
	SEEK_SET,
};

/** Data transfer metadata */
struct xfer_metadata {
	/** Position of data within stream */
	off_t offset;
	/** Basis for data position
	 *
	 * Must be one of @c SEEK_CUR or @c SEEK_SET.
	 */
	int whence;
	/** Source socket address, or NULL */
	struct sockaddr *src;
	/** Destination socket address, or NULL */
	struct sockaddr *dest;
	/** Network device, or NULL */
	struct net_device *netdev;
};

/**
 * Describe seek basis
 *
 * @v whence		Basis for new position
 */
static inline __attribute__ (( always_inline )) const char *
whence_text ( int whence ) {
	switch ( whence ) {
	case SEEK_CUR:	return "CUR";
	case SEEK_SET:	return "SET";
	default:	return "INVALID";
	}
}

extern struct xfer_interface null_xfer;
extern struct xfer_interface_operations null_xfer_ops;

extern void xfer_close ( struct xfer_interface *xfer, int rc );
extern int xfer_vredirect ( struct xfer_interface *xfer, int type,
			    va_list args );
extern int xfer_redirect ( struct xfer_interface *xfer, int type, ... );
extern size_t xfer_window ( struct xfer_interface *xfer );
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
extern int __attribute__ (( format ( printf, 2, 3 ) ))
xfer_printf ( struct xfer_interface *xfer, const char *format, ... );
extern int xfer_seek ( struct xfer_interface *xfer, off_t offset, int whence );

extern void ignore_xfer_close ( struct xfer_interface *xfer, int rc );
extern int ignore_xfer_vredirect ( struct xfer_interface *xfer,
				   int type, va_list args );
extern size_t unlimited_xfer_window ( struct xfer_interface *xfer );
extern size_t no_xfer_window ( struct xfer_interface *xfer );
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
 * Initialise a static data transfer interface
 *
 * @v operations		Data transfer interface operations
 */
#define XFER_INIT( operations ) {			\
		.intf = {				\
			.dest = &null_xfer.intf,	\
			.refcnt = NULL,			\
		},					\
		.op = operations,			\
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
