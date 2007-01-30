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

	/* Send data via connection */
	if ( ( rc = conn->op->send ( conn, data, len ) ) != 0 ) {
		DBGC ( app, "Stream %p failed to send %zd bytes: %s\n",
		       app, len, strerror ( rc ) );
		return rc;
	}

	return 0;
}
