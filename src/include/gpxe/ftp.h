#ifndef _GPXE_FTP_H
#define _GPXE_FTP_H

/** @file
 *
 * File transfer protocol
 *
 */

#include <stdint.h>
#include <gpxe/async.h>
#include <gpxe/tcp.h>

struct buffer;

/** FTP default port */
#define FTP_PORT 21

/**
 * FTP states
 *
 * These @b must be sequential, i.e. a successful FTP session must
 * pass through each of these states in order.
 */
enum ftp_state {
	FTP_CONNECT = 0,
	FTP_USER,
	FTP_PASS,
	FTP_TYPE,
	FTP_PASV,
	FTP_RETR,
	FTP_QUIT,
	FTP_DONE,
};

/**
 * An FTP request
 *
 */
struct ftp_request {
	/** URI being fetched */
	struct uri *uri;
	/** Data buffer to fill */
	struct buffer *buffer;
	/** Asynchronous operation */
	struct async async;

	/** Current state */
	enum ftp_state state;
	/** Amount of current message already transmitted */
	size_t already_sent;
	/** Buffer to be filled with data received via the control channel */
	char *recvbuf;
	/** Remaining size of recvbuf */
	size_t recvsize;
	/** FTP status code, as text */
	char status_text[4];
	/** Passive-mode parameters, as text */
	char passive_text[24]; /* "aaa,bbb,ccc,ddd,eee,fff" */
	/** TCP application for the control channel */
	struct tcp_application tcp;
	/** TCP application for the data channel */
	struct tcp_application tcp_data;
};

extern int ftp_get ( struct uri *uri, struct buffer *buffer,
		     struct async *parent );

#endif /* _GPXE_FTP_H */
