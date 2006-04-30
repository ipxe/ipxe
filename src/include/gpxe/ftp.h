#ifndef _GPXE_FTP_H
#define _GPXE_FTP_H

/** @file
 *
 * File transfer protocol
 *
 */

#include <stdint.h>
#include <gpxe/tcp.h>

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
	/** TCP connection for this request */
	struct tcp_connection tcp;
	/** File to download */
	const char *filename;
	/** Callback function
	 *
	 * @v data	Received data
	 * @v len	Length of received data
	 *
	 * This function is called for all data received from the
	 * remote server.
	 */
	void ( *callback ) ( char *data, size_t len );
	/** Completion indicator
	 *
	 * This will be set to a non-zero value when the transfer is
	 * complete.  A negative value indicates an error.
	 */
	int complete;

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

	/** TCP connection for the data channel */
	struct tcp_connection tcp_data;
};

extern void ftp_connect ( struct ftp_request *ftp );

#endif
