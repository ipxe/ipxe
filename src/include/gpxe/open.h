#ifndef _GPXE_OPEN_H
#define _GPXE_OPEN_H

/** @file
 *
 * Data transfer interface opening
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdarg.h>
#include <gpxe/tables.h>
#include <gpxe/socket.h>

struct xfer_interface;
struct uri;

/** Location types */
enum {
	/** Location is a URI
	 *
	 * Parameter list for open() is:
	 *
	 * struct uri *uri;
	 */
	LOCATION_URI = 1,
	/** Location is a URI string
	 *
	 * Parameter list for open() is:
	 *
	 * const char *uri_string;
	 */
	LOCATION_URI_STRING,
	/** Location is a socket
	 *
	 * Parameter list for open() is:
	 *
	 * int semantics;
	 * struct sockaddr *peer;
	 * struct sockaddr *local;
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
	 * @v xfer		Data transfer interface
	 * @v uri		URI
	 * @ret rc		Return status code
	 */
	int ( * open ) ( struct xfer_interface *xfer, struct uri *uri );
};

/** URI opener table */
#define URI_OPENERS __table ( struct uri_opener, "uri_openers" )

/** Register a URI opener */
#define __uri_opener __table_entry ( URI_OPENERS, 01 )

/** A socket opener */
struct socket_opener {
	/** Communication semantics (e.g. SOCK_STREAM) */
	int semantics;
	/** Address family (e.g. AF_INET) */
	int family;
	/** Open socket
	 *
	 * @v xfer		Data transfer interface
	 * @v peer		Peer socket address
	 * @v local		Local socket address, or NULL
	 * @ret rc		Return status code
	 */
	int ( * open ) ( struct xfer_interface *xfer, struct sockaddr *peer,
			 struct sockaddr *local );
};

/** Socket opener table */
#define SOCKET_OPENERS __table ( struct socket_opener, "socket_openers" )

/** Register a socket opener */
#define __socket_opener __table_entry ( SOCKET_OPENERS, 01 )

extern int xfer_open_uri ( struct xfer_interface *xfer, struct uri *uri );
extern int xfer_open_uri_string ( struct xfer_interface *xfer,
				  const char *uri_string );
extern int xfer_open_named_socket ( struct xfer_interface *xfer,
				    int semantics, struct sockaddr *peer,
				    const char *name, struct sockaddr *local );
extern int xfer_open_socket ( struct xfer_interface *xfer, int semantics,
			      struct sockaddr *peer, struct sockaddr *local );
extern int xfer_vopen ( struct xfer_interface *xfer, int type, va_list args );
extern int xfer_open ( struct xfer_interface *xfer, int type, ... );
extern int xfer_vreopen ( struct xfer_interface *xfer, int type,
			  va_list args );

#endif /* _GPXE_OPEN_H */
