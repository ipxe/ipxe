#ifndef _GPXE_STREAM_H
#define _GPXE_STREAM_H

/** @file
 *
 * Stream API
 */

#include <stdint.h>
#include <gpxe/socket.h>

struct stream_application;
struct stream_connection;

/** Stream applicatin-layer operations */
struct stream_application_operations {
	/**
	 * Connection established
	 *
	 * @v app		Stream application
	 */
	void ( * connected ) ( struct stream_application *app );
	/**
	 * Connection closed
	 *
	 * @v app		Stream application
	 * @v rc		Error code, if any
	 *
	 * This is called when the connection is closed for any
	 * reason, including timeouts or aborts.  The error code
	 * contains the negative error number, if the closure is due
	 * to an error, or zero for a normal close.
	 *
	 * When closed() is called, the application no longer has a
	 * valid stream connection.  Note that connected() may not
	 * have been called before closed(), if the close is due to an
	 * error during connection setup.
	 */
	void ( * closed ) ( struct stream_application *app, int rc );
	/**
	 * Transmit data
	 *
	 * @v app		Stream application
	 * @v data		Temporary data buffer
	 * @v len		Length of temporary data buffer
	 *
	 * The application should transmit whatever it currently wants
	 * to send using stream_send().  If retransmissions are
	 * required, senddata() will be called again and the
	 * application must regenerate the data.  The easiest way to
	 * implement this is to ensure that senddata() never changes
	 * the application's state.
	 *
	 * The application may use the temporary data buffer to
	 * construct the data to be sent.  Note that merely filling
	 * the buffer will do nothing; the application must call
	 * stream_send() in order to actually transmit the data.  Use
	 * of the buffer is not compulsory; the application may call
	 * stream_send() on any block of data.
	 */
	void ( * senddata ) ( struct stream_application *app,
			      void *data, size_t len );
	/**
	 * Transmitted data acknowledged
	 *
	 * @v app		Stream application
	 * @v len		Length of acknowledged data
	 *
	 * @c len is guaranteed to not exceed the outstanding amount
	 * of unacknowledged data.
	 */
	void ( * acked ) ( struct stream_application *app, size_t len );
	/**
	 * Receive new data
	 *
	 * @v app		Stream application
	 * @v data		Data
	 * @v len		Length of data
	 */
	void ( * newdata ) ( struct stream_application *app,
			     void *data, size_t len );
};

/** Stream connection-layer operations */
struct stream_connection_operations {
	/**
	 * Bind to local address
	 *
	 * @v conn		Stream connection
	 * @v local		Local address
	 * @ret rc		Return status code
	 */
	int ( * bind ) ( struct stream_connection *conn,
			 struct sockaddr *local );
	/**
	 * Connect to remote address
	 *
	 * @v conn		Stream connection
	 * @v peer		Remote address
	 * @ret rc		Return status code
	 *
	 * This initiates the connection.  If the connection succeeds,
	 * the application's connected() method will be called.  If
	 * the connection fails (e.g. due to a timeout), the
	 * application's closed() method will be called with an
	 * appropriate error code.
	 */
	int ( * connect ) ( struct stream_connection *conn,
			    struct sockaddr *peer );
	/**
	 * Close connection
	 *
	 * @v conn		Stream connection
	 */
	void ( * close ) ( struct stream_connection *conn );
	/**
	 * Send data via connection
	 *
	 * @v conn		Stream connection
	 * @v data		Data to send
	 * @v len		Length of data
	 * @ret rc		Return status code
	 *
	 * This method should be called only in the context of an
	 * application's senddata() method.
	 */
	int ( * send ) ( struct stream_connection *conn,
			 const void *data, size_t len );
	/**
	 * Notify connection that data is available to send
	 *
	 * @v conn		Stream connection
	 * @ret rc		Return status code
	 *
	 * This will cause the connection to call the application's
	 * senddata() method.  It should be called when the
	 * application acquires new data to send as a result of
	 * something external to the data stream (e.g. when iSCSI is
	 * asked to issue a new command on an otherwise-idle
	 * connection).  Most applications will not need to call this
	 * method.
	 */
	int ( * kick ) ( struct stream_connection *conn );
};

/** A stream application */
struct stream_application {
	/** Stream connection, if any
	 *
	 * This will be NULL if the stream does not currently have a
	 * valid connection.
	 */
	struct stream_connection *conn;
	/** Stream application-layer operations */
	struct stream_application_operations *op;
};

/** A stream connection */
struct stream_connection {
	/** Stream application, if any
	 *
	 * This will be NULL if the stream does not currently have a
	 * valid application.
	 */
	struct stream_application *app;
	/** Stream connection-layer operations */
	struct stream_connection_operations *op;	
};

extern void stream_associate ( struct stream_application *app,
			       struct stream_connection *conn );

extern void stream_connected ( struct stream_connection *conn );
extern void stream_closed ( struct stream_connection *conn, int rc );
extern void stream_senddata ( struct stream_connection *conn,
			      void *data, size_t len );
extern void stream_acked ( struct stream_connection *conn, size_t len );
extern void stream_newdata ( struct stream_connection *conn,
			     void *data, size_t len );

extern int stream_bind ( struct stream_application *app,
			 struct sockaddr *local );
extern int stream_connect ( struct stream_application *app,
			    struct sockaddr *peer );
extern void stream_close ( struct stream_application *app );
extern int stream_send ( struct stream_application *app,
			 const void *data, size_t len );
extern int stream_kick ( struct stream_application *app );

#endif /* _GPXE_STREAM_H */
