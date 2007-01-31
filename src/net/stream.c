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
 * Stream API
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <gpxe/stream.h>

/**
 * Connection established
 *
 * @v conn		Stream connection
 */
void stream_connected ( struct stream_connection *conn ) {
	struct stream_application *app = conn->app;

	DBGC ( app, "Stream %p connected\n", app );

	/* Check connection actually exists */
	if ( ! app ) {
		DBGC ( conn, "Stream connection %p has no application\n",
		       conn );
		return;
	}

	/* Hand off to application */
	app->op->connected ( app );
}

/**
 * Connection closed
 *
 * @v conn		Stream connection
 * @v rc		Error code, if any
 */
void stream_closed ( struct stream_connection *conn, int rc ) {
	struct stream_application *app = conn->app;

	DBGC ( app, "Stream %p closed (%s)\n", app, strerror ( rc ) );

	/* Check connection actually exists */
	if ( ! app ) {
		DBGC ( conn, "Stream connection %p has no application\n",
		       conn );
		return;
	}

	/* Hand off to application */
	app->op->closed ( app, rc );
}

/**
 * Transmit data
 *
 * @v conn		Stream connection
 * @v data		Temporary data buffer
 * @v len		Length of temporary data buffer
 */
void stream_senddata ( struct stream_connection *conn,
		       void *data, size_t len ) {
	struct stream_application *app = conn->app;

	DBGC2 ( app, "Stream %p sending data\n", app );

	/* Check connection actually exists */
	if ( ! app ) {
		DBGC ( conn, "Stream connection %p has no application\n",
		       conn );
		return;
	}

	/* Hand off to application */
	app->op->senddata ( app, data, len );
}

/**
 * Transmitted data acknowledged
 *
 * @v conn		Stream connection
 * @v len		Length of acknowledged data
 *
 * @c len must not exceed the outstanding amount of unacknowledged
 * data.
 */
void stream_acked ( struct stream_connection *conn, size_t len ) {
	struct stream_application *app = conn->app;

	DBGC2 ( app, "Stream %p had %zd bytes acknowledged\n", app, len );

	/* Check connection actually exists */
	if ( ! app ) {
		DBGC ( conn, "Stream connection %p has no application\n",
		       conn );
		return;
	}

	/* Hand off to application */
	app->op->acked ( app, len );
}

/**
 * Receive new data
 *
 * @v conn		Stream connection
 * @v data		Data
 * @v len		Length of data
 */
void stream_newdata ( struct stream_connection *conn, 
		      void *data, size_t len ) {
	struct stream_application *app = conn->app;

	DBGC2 ( app, "Stream %p received %zd bytes\n", app, len );

	/* Check connection actually exists */
	if ( ! app ) {
		DBGC ( conn, "Stream connection %p has no application\n",
		       conn );
		return;
	}

	/* Hand off to application */
	app->op->newdata ( app, data, len );
}

/**
 * Bind to local address
 *
 * @v app		Stream application
 * @v local		Local address
 * @ret rc		Return status code
 */
int stream_bind ( struct stream_application *app, struct sockaddr *local ) {
	struct stream_connection *conn = app->conn;
	int rc;

	DBGC2 ( app, "Stream %p binding\n", app );

	/* Check connection actually exists */
	if ( ! conn ) {
		DBGC ( app, "Stream %p has no connection\n", app );
		return -ENOTCONN;
	}

	/* Hand off to connection */
	if ( ( rc = conn->op->bind ( conn, local ) ) != 0 ) {
		DBGC ( app, "Stream %p failed to bind: %s\n",
		       app, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Connect to remote address
 *
 * @v app		Stream application
 * @v peer		Remote address
 * @ret rc		Return status code
 */
int stream_connect ( struct stream_application *app, struct sockaddr *peer ) {
	struct stream_connection *conn = app->conn;
	int rc;

	DBGC2 ( app, "Stream %p connecting\n", app );

	/* Check connection actually exists */
	if ( ! conn ) {
		DBGC ( app, "Stream %p has no connection\n", app );
		return -ENOTCONN;
	}

	/* Hand off to connection */
	if ( ( rc = conn->op->connect ( conn, peer ) ) != 0 ) {
		DBGC ( app, "Stream %p failed to connect: %s\n",
		       app, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Close connection
 *
 * @v app		Stream application
 */
void stream_close ( struct stream_application *app ) {
	struct stream_connection *conn = app->conn;

	DBGC2 ( app, "Stream %p closing\n", app );

	/* Check connection actually exists */
	if ( ! conn ) {
		DBGC ( app, "Stream %p has no connection\n", app );
		return;
	}

	/* Hand off to connection */
	conn->op->close ( conn );
}

/**
 * Send data via connection
 *
 * @v app		Stream application
 * @v data		Data to send
 * @v len		Length of data
 * @ret rc		Return status code
 *
 * This method should be called only in the context of an
 * application's senddata() method.
 */
int stream_send ( struct stream_application *app, void *data, size_t len ) {
	struct stream_connection *conn = app->conn;
	int rc;

	DBGC2 ( app, "Stream %p sending %zd bytes\n", app, len );

	/* Check connection actually exists */
	if ( ! conn ) {
		DBGC ( app, "Stream %p has no connection\n", app );
		return -ENOTCONN;
	}

	/* Hand off to connection */
	if ( ( rc = conn->op->send ( conn, data, len ) ) != 0 ) {
		DBGC ( app, "Stream %p failed to send %zd bytes: %s\n",
		       app, len, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Notify connection that data is available to send
 *
 * @v app		Stream application
 * @ret rc		Return status code
 */
int stream_kick ( struct stream_application *app ) {
		struct stream_connection *conn = app->conn;
	int rc;

	DBGC2 ( app, "Stream %p kicking connection\n", app );

	/* Check connection actually exists */
	if ( ! conn ) {
		DBGC ( app, "Stream %p has no connection\n", app );
		return -ENOTCONN;
	}

	/* Hand off to connection */
	if ( ( rc = conn->op->kick ( conn ) ) != 0 ) {
		DBGC ( app, "Stream %p failed to kick connection: %s\n",
		       app, strerror ( rc ) );
		return rc;
	}

	return 0;
}
