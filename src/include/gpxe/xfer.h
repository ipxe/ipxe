#ifndef _GPXE_XFER_H
#define _GPXE_XFER_H

/** @file
 *
 * Data transfer interfaces
 *
 */

#include <stddef.h>
#include <gpxe/interface.h>
#include <gpxe/iobuf.h>

struct xfer_interface;

/** Data transfer interface operations */
struct xfer_interface_operations {
	/** Deliver datagram
	 *
	 * @v xfer		Data-transfer interface
	 * @v src		Source interface
	 * @v iobuf		Datagram I/O buffer
	 * @ret rc		Return status code
	 *
	 * A data transfer interface that wishes to support only raw
	 * data delivery should set this method to
	 * deliver_as_raw().
	 */
	int ( * deliver ) ( struct xfer_interface *xfer,
			    struct xfer_interface *src,
			    struct io_buffer *iobuf );
	/** Deliver datagram as raw data
	 *
	 * @v xfer		Data-transfer interface
	 * @v src		Source interface
	 * @v data		Data buffer
	 * @v len		Length of data buffer
	 * @ret rc		Return status code
	 *
	 * A data transfer interface that wishes to support only I/O
	 * buffer delivery should set this method to
	 * deliver_as_iobuf().
	 */
	int ( * deliver_raw ) ( struct xfer_interface *xfer,
				struct xfer_interface *src,
				const void *data, size_t len );
};

/** A data transfer interface */
struct xfer_interface {
	/** Generic object communication interface */
	struct interface intf;
	/** Operations for received messages */
	struct xfer_interface_operations *op;
};

extern struct xfer_interface null_xfer;
extern struct xfer_interface_operations null_xfer_ops;

extern int deliver_as_raw ( struct xfer_interface *xfer,
			    struct xfer_interface *src,
			    struct io_buffer *iobuf );
extern int deliver_as_iobuf ( struct xfer_interface *xfer,
			      struct xfer_interface *src,
			      const void *data, size_t len );

/**
 * Get destination data-transfer interface
 *
 * @v xfer		Data-transfer interface
 * @ret dest		Destination interface
 */
static inline struct xfer_interface *
xfer_dest ( struct xfer_interface *xfer ) {
	return container_of ( xfer->intf.dest, struct xfer_interface, intf );
}

/**
 * Plug a data-transfer interface into a new destination interface
 *
 * @v xfer		Data-transfer interface
 * @v dest		New destination interface
 */
static inline void xfer_plug ( struct xfer_interface *xfer,
			       struct xfer_interface *dest ) {
	plug ( &xfer->intf, &dest->intf );
}

/**
 * Unplug a data-transfer interface
 *
 * @v xfer		Data-transfer interface
 */
static inline void xfer_unplug ( struct xfer_interface *xfer ) {
	plug ( &xfer->intf, &null_xfer.intf );
}

/**
 * Terminate a data-transfer interface
 *
 * @v xfer		Data-transfer interface
 *
 * After calling this method, no further messages will be received via
 * the interface.
 */
static inline void xfer_terminate ( struct xfer_interface *xfer ) {
	xfer->op = &null_xfer_ops;
};

#endif /* _GPXE_XFER_H */
