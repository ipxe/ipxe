#ifndef _GPXE_FILTER_H
#define _GPXE_FILTER_H

/** @file
 *
 * Filter streams
 */

#include <gpxe/stream.h>

/** A filter stream */
struct filter_stream {
	/** Downstream
	 *
	 * This is the end pointing towards the bottom-level
	 * connection (e.g. TCP).
	 */
	struct stream_application downstream;
	/** Upstream
	 *
	 * This is the end pointing towards the top-level application
	 * (e.g. HTTP).
	 */
	struct stream_connection upstream;
};

extern void filter_connected ( struct stream_application *app );
extern void filter_closed ( struct stream_application *app, int rc );
extern void filter_senddata ( struct stream_application *app,
			      void *data, size_t len );
extern void filter_acked ( struct stream_application *app, size_t len );
extern void filter_newdata ( struct stream_application *app,
			     void *data, size_t len );

extern int filter_bind ( struct stream_connection *conn,
			 struct sockaddr *local );
extern int filter_connect ( struct stream_connection *conn,
			    struct sockaddr *peer );
extern void filter_close ( struct stream_connection *conn );
extern int filter_send ( struct stream_connection *conn,
			 void *data, size_t len );
extern int filter_kick ( struct stream_connection *conn );

extern int insert_filter ( struct stream_application *app,
			   struct filter_stream *filter );

#endif /* _GPXE_FILTER_H */
