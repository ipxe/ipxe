#ifndef _IPXE_XFER_H
#define _IPXE_XFER_H

/** @file
 *
 * Data transfer interfaces
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stddef.h>
#include <stdarg.h>
#include <ipxe/interface.h>

struct xfer_metadata;
struct io_buffer;
struct sockaddr;
struct net_device;

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

/* Data transfer interface operations */

extern int xfer_vredirect ( struct interface *intf, int type,
			    va_list args );
#define xfer_vredirect_TYPE( object_type ) \
	typeof ( int ( object_type, int type, va_list args ) )

extern size_t xfer_window ( struct interface *intf );
#define xfer_window_TYPE( object_type ) \
	typeof ( size_t ( object_type ) )

extern struct io_buffer * xfer_alloc_iob ( struct interface *intf,
					   size_t len );
#define xfer_alloc_iob_TYPE( object_type ) \
	typeof ( struct io_buffer * ( object_type, size_t len ) )

extern int xfer_deliver ( struct interface *intf,
			  struct io_buffer *iobuf,
			  struct xfer_metadata *meta );
#define xfer_deliver_TYPE( object_type )			\
	typeof ( int ( object_type, struct io_buffer *iobuf,	\
		       struct xfer_metadata *meta ) )

/* Data transfer interface helper functions */

extern int xfer_redirect ( struct interface *xfer, int type, ... );
extern int xfer_deliver_iob ( struct interface *intf,
			      struct io_buffer *iobuf );
extern int xfer_deliver_raw ( struct interface *intf,
			      const void *data, size_t len );
extern int xfer_vprintf ( struct interface *intf,
			  const char *format, va_list args );
extern int __attribute__ (( format ( printf, 2, 3 ) ))
xfer_printf ( struct interface *intf, const char *format, ... );
extern int xfer_seek ( struct interface *intf, off_t offset, int whence );

#endif /* _IPXE_XFER_H */
