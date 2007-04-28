#ifndef _GPXE_OPEN_H
#define _GPXE_OPEN_H

/** @file
 *
 * Data transfer interface opening
 *
 */

#include <gpxe/tables.h>

struct xfer_interface;
struct uri;
struct sockaddr;

/** Location types */
enum {
	/** Location is a URI string
	 *
	 * Parameter list for open() is:
	 *
	 * const char *uri_string;
	 */
	LOCATION_URI = 1,
	/** Location is a socket
	 *
	 * Parameter list for open() is:
	 *
	 * 
	 */
	LOCATION_SOCKET,
};

/** A URI opener */
struct uri_opener {
	/** URI protocol name
	 *
	 * This is the "scheme" portion of the URI, e.g. "http" or
	 * "file".
	 */
	const char *scheme;
	/** Open URI
	 *
	 * @v xfer		Data-transfer interface
	 * @v uri		URI
	 * @ret rc		Return status code
	 *
	 * This method takes ownership of the URI structure, and is
	 * responsible for eventually calling free_uri().
	 */
	int ( * open ) ( struct xfer_interface *xfer, struct uri *uri );
};

/** Register a URI opener */
#define __uri_opener __table ( struct uri_opener, uri_openers, 01 )

/** A socket opener */
struct socket_opener {
	/** Communication domain (e.g. PF_INET) */
	int domain;
	/** Communication semantics (e.g. SOCK_STREAM) */
	int type;
	/** Open socket
	 *
	 * @v xfer		Data-transfer interface
	 * @v sa		Socket address
	 * @ret rc		Return status code
	 */
	int ( * open ) ( struct xfer_interface *xfer, struct sockaddr *sa );
};

/** Register a socket opener */
#define __socket_opener __table ( struct socket_opener, socket_openers, 01 )

extern int open_uri ( struct xfer_interface *xfer, const char *uri_string );
extern int open_socket ( struct xfer_interface *xfer,
			 int domain, int type, struct sockaddr *sa );
extern int vopen ( struct xfer_interface *xfer, int type, va_list args );
extern int open ( struct xfer_interface *xfer, int type, ... );

#endif /* _GPXE_OPEN_H */
