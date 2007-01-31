/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * @file
 *
 * Filter streams
 */

#include <stddef.h>
#include <errno.h>
#include <gpxe/stream.h>
#include <gpxe/filter.h>

/**
 * Connection established
 *
 * @v app		Stream application
 */
void filter_connected ( struct stream_application *app ) {
	struct filter_stream *filter = 
		container_of ( app, struct filter_stream, downstream );

	stream_connected ( &filter->upstream );
}

/**
 * Connection closed
 *
 * @v app		Stream application
 * @v rc		Error code, if any
 */
void filter_closed ( struct stream_application *app, int rc ) {
	struct filter_stream *filter = 
		container_of ( app, struct filter_stream, downstream );

	stream_closed ( &filter->upstream, rc );
}

/**
 * Transmit data
 *
 * @v app		Stream application
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 */
void filter_senddata ( struct stream_application *app,
		       void *data, size_t len ) {
	struct filter_stream *filter = 
		container_of ( app, struct filter_stream, downstream );

	stream_senddata ( &filter->upstream, data, len );
}

/**
 * Transmitted data acknowledged
 *
 * @v app		Stream application
 * @v len		Length of acknowledged data
 */
void filter_acked ( struct stream_application *app, size_t len ) {
	struct filter_stream *filter = 
		container_of ( app, struct filter_stream, downstream );

	stream_acked ( &filter->upstream, len );
}

/**
 * Receive new data
 *
 * @v app		Stream application
 * @v data		Data
 * @v len		Length of data
 */
void filter_newdata ( struct stream_application *app,
		      void *data, size_t len ) {
	struct filter_stream *filter = 
		container_of ( app, struct filter_stream, downstream );

	stream_newdata ( &filter->upstream, data, len );
}

/**
 * Bind to local address
 *
 * @v conn		Stream connection
 * @v local		Local address
 * @ret rc		Return status code
 */
int filter_bind ( struct stream_connection *conn, struct sockaddr *local ) {
	struct filter_stream *filter = 
		container_of ( conn, struct filter_stream, upstream );

	return stream_bind ( &filter->downstream, local );
}

/**
 * Connect to remote address
 *
 * @v conn		Stream connection
 * @v peer		Remote address
 * @ret rc		Return status code
 */
int filter_connect ( struct stream_connection *conn, struct sockaddr *peer ) {
	struct filter_stream *filter = 
		container_of ( conn, struct filter_stream, upstream );

	return stream_connect ( &filter->downstream, peer );
}

/**
 * Close connection
 *
 * @v conn		Stream connection
 */
void filter_close ( struct stream_connection *conn ) {
	struct filter_stream *filter = 
		container_of ( conn, struct filter_stream, upstream );

	stream_close ( &filter->downstream );
}

/**
 * Send data via connection
 *
 * @v conn		Stream connection
 * @v data		Data to send
 * @v len		Length of data
 * @ret rc		Return status code
 */
int filter_send ( struct stream_connection *conn, void *data, size_t len ) {
	struct filter_stream *filter = 
		container_of ( conn, struct filter_stream, upstream );

	return stream_send ( &filter->downstream, data, len );
}

/**
 * Notify connection that data is available to send
 *
 * @v conn		Stream connection
 * @ret rc		Return status code
 */
int filter_kick ( struct stream_connection *conn ) {
	struct filter_stream *filter = 
		container_of ( conn, struct filter_stream, upstream );

	return stream_kick ( &filter->downstream );
}

/**
 * Insert filter into stream
 *
 * @v app		Stream application
 * @v filter		Filter stream
 * @ret rc		Return status code
 */
int insert_filter ( struct stream_application *app,
		    struct filter_stream *filter ) {
	struct stream_connection *conn = app->conn;

	if ( ! conn ) {
		DBGC ( filter, "Filter %p cannot insert onto closed stream\n",
		       filter );
		return -ENOTCONN;
	}

	DBGC ( filter, "Filter %p inserted on stream %p\n", filter, app );

	filter->upstream.app = app;
	filter->downstream.conn = conn;
	app->conn = &filter->upstream;
	conn->app = &filter->downstream;

	return 0;
}
