#ifndef _GPXE_HTTP_H
#define _GPXE_HTTP_H

/** @file
 *
 * Hyper Text Transport Protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** HTTP default port */
#define HTTP_PORT 80

/** HTTPS default port */
#define HTTPS_PORT 443

extern int http_open_filter ( struct xfer_interface *xfer, struct uri *uri,
			      unsigned int default_port,
			      int ( * filter ) ( struct xfer_interface *,
						 struct xfer_interface ** ) );

#endif /* _GPXE_HTTP_H */
