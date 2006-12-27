#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <vsprintf.h>
#include <assert.h>
#include <gpxe/async.h>
#include <gpxe/http.h>

/** @file
 *
 * The Hyper Text Transfer Protocol (HTTP)
 *
 * This file implements the TCP-based HTTP protocol. It connects to the
 * server specified in http_request::tcp and transmit an HTTP GET request
 * for the file specified in http_request::filename. It then decoded the
 * HTTP header, determining file status and file size. Then sends the file
 * to the callback function at http_request::callback().
 * **NOTE**: still working on correcting the closing of the tcp connection
 *
 * To use this code, do something like:
 *
 * @code
 *
 *   static void my_callback ( struct http_request *http, char *data, size_t len ) {
 *     ... process data ...
 *   }
 *
 *   struct http_request http = {
 *     .filename = "path/to/file",
 *     .callback = my_callback,
 *   };
 *
 *   ... assign http.tcp.server ...
 *
 *   rc = async_wait ( get_http ( &http ) );
 *
 * @endcode
 *
 */

static inline struct http_request *
tcp_to_http ( struct tcp_application *app ) {
	return container_of ( app, struct http_request, tcp );
}

/**
 * Close an HTTP connection
 *
 * @v app	a TCP Application
 * @v status	connection status at close
 */
static void http_closed ( struct tcp_application *app, int status ) {
	struct http_request *http = tcp_to_http ( app );
	async_done ( &http->aop, status );
}

/**
 * Callback after a TCP connection is established
 *
 * @v app	a TCP Application
 */
static void http_connected ( struct tcp_application *app ) {
	struct http_request *http = tcp_to_http ( app );

	http->state = HTTP_REQUEST_FILE;
}

/**
 * Callback for when TCP data is acknowledged
 *
 * @v app	a TCP Application
 * @v len	the length of data acked
 */
static void http_acked ( struct tcp_application *app, size_t len __attribute__ ((unused)) ) {
	struct http_request *http = tcp_to_http ( app );

	// assume that the whole GET request was sent in on epacket

	switch ( http->state ) {
	case HTTP_REQUEST_FILE:
		http->state = HTTP_PARSE_HEADER;
		break;
	case HTTP_PARSE_HEADER:
	case HTTP_RECV_FILE:
		break;
	case HTTP_DONE:
		//tcp_close(app);
		break;
	default:
		break;
	}
	//printf("acked\n");
}

/**
 * Callback when new TCP data is recieved
 *
 * @v app	a TCP Application
 * @v data	a pointer to the data recieved
 * @v len	length of data buffer
 */
static void http_newdata ( struct tcp_application *app, void *data,
			    size_t len ) {
	struct http_request *http = tcp_to_http ( app );
	char *content_length;
	char *start = data;
	char *rcp; int rc;

	switch ( http->state ) {
	case HTTP_PARSE_HEADER:
		if(strncmp("HTTP/",data,5) != 0){
			// no http header
			printf("Error: no HTTP Header\n");
		}
		// if rc is not 200, then handle problem
		// either redirect or not there
		rcp = strstr(data,"HTTP");
		if(rcp == NULL){ printf("Could not find header status line.\n"); }
		rcp += 9;
		rc = strtoul(rcp,NULL,10);
		printf("RC=%d\n",rc);
		content_length = strstr(data,"Content-Length: ");
		if(content_length != NULL){
			content_length += 16;
			http->file_size = strtoul(content_length,NULL,10);
			http->file_recv = 0;
			printf("http->file_size = %d\n", http->file_size);
		}
		start = strstr(data,"\r\n\r\n");
		if(start == NULL){ printf("No end of header\n"); }
		else{
			start += 4;
			len -= ((void *)start - data);
			http->state = HTTP_RECV_FILE;
		}

		if ( http->state != HTTP_RECV_FILE )
			break;
	case HTTP_RECV_FILE:
		http->callback(http,start,len);
		//http->file_size -= len;
		//printf("File recv is %d\n", http->file_recv);
		if ( http->file_recv == http->file_size ){
			http->state = HTTP_DONE;
			tcp_close(app);
		}
		break;
	case HTTP_REQUEST_FILE:
	case HTTP_DONE:
	default:
		break;
	}
}

/**
 * Callback for sending TCP data
 *
 * @v app	a TCP Application
 */
static void http_senddata ( struct tcp_application *app, void *buf, size_t len ) {
	struct http_request *http = tcp_to_http ( app );

	switch ( http->state ){
	case HTTP_REQUEST_FILE:
		len = snprintf(buf,len,"GET %s HTTP/1.0\r\n\r\n",http->filename);
		printf("%s\n",(char *)buf);
        	// string is: GET <file> HTTP/1.0\r\n\r\n

		tcp_send ( app, buf, len);
		break;
	case HTTP_PARSE_HEADER:
	case HTTP_RECV_FILE:
		break;
	case HTTP_DONE:
		//tcp_close(app)
		break;
	default:
		break;
	}
}

static struct tcp_operations http_tcp_operations = {
	.closed		= http_closed,
	.connected	= http_connected,
	.acked		= http_acked,
	.newdata	= http_newdata,
	.senddata	= http_senddata,
};

/**
 * Initiate a HTTP connection
 *
 * @v http	a HTTP request
 */
struct async_operation * get_http ( struct http_request *http ) {
	int rc;

	http->tcp.tcp_op = &http_tcp_operations;
	http->state = HTTP_REQUEST_FILE;
	if ( ( rc = tcp_connect ( &http->tcp, &http->server, 0 ) ) != 0 )
		async_done ( &http->aop, rc );

	return &http->aop;
}
