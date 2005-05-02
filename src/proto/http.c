#include "proto.h"
#include "tcp.h"
#include "url.h"
#include "etherboot.h"

/* The block size is currently chosen to be 512 bytes. This means, we can
   allocate the receive buffer on the stack, but it results in a noticeable
   performance penalty.
   This is what needs to be done in order to increase the block size:
     - size negotiation needs to be implemented in TCP
     - the buffer needs to be allocated on the heap
     - path MTU discovery needs to be implemented
*/ /***/ /* FIXME */
#define BLOCKSIZE TFTP_DEFAULTSIZE_PACKET

/**************************************************************************
SEND_TCP_CALLBACK - Send data using TCP
**************************************************************************/
struct send_recv_state {
	int ( * process ) ( unsigned char *data,
			    unsigned int blocknum,
			    unsigned int len, int eof );
	char *send_buffer;
	char *recv_buffer;
	int send_length;
	int recv_length;
	int bytes_sent;
	int block;
	int bytes_received;
	enum { RESULT_CODE, HEADER, DATA, ERROR, MOVED } recv_state;
	int rc;
	char *url;
};

static int send_tcp_request(int length, void *buffer, void *ptr) {
	struct send_recv_state *state = (struct send_recv_state *)ptr;

	if (length > state->send_length - state->bytes_sent)
		length = state->send_length - state->bytes_sent;
	memcpy(buffer, state->send_buffer + state->bytes_sent, length);
	state->bytes_sent += length;
	return (length);
}

/**************************************************************************
RECV_TCP_CALLBACK - Receive data using TCP
**************************************************************************/
static int recv_tcp_request(int length, const void *buffer, void *ptr) {
	struct send_recv_state *state = (struct send_recv_state *)ptr;

	/* Assume that the lines in an HTTP header do not straddle a packet */
	/* boundary. This is probably a reasonable assumption */
	if (state->recv_state == RESULT_CODE) {
		while (length > 0) {
			/* Find HTTP result code */
			if (*(const char *)buffer == ' ') {
				const char *ptr = ((const char *)buffer) + 1;
				int rc = strtoul(ptr, &ptr, 10);
				if (ptr >= (const char *)buffer + length) {
					state->recv_state = ERROR;
					DBG ( "HTTP got bad result code\n" );
					return 0;
				}
				state->rc = rc;
				state->recv_state = HEADER;
				DBG ( "HTTP got result code %d\n", rc );
				goto header;
			}
			++(const char *)buffer;
			length--;
		}
		state->recv_state = ERROR;
		DBG ( "HTTP got no result code\n" );
		return 0;
	}
	if (state->recv_state == HEADER) {
	header: while (length > 0) {
			/* Check for HTTP redirect */
			if (state->rc >= 300 && state->rc < 400 &&
			    !memcmp(buffer, "Location: ", 10)) {
				char *p;
				
				state->url = p = ( char * ) buffer + 10;
				while ( *p > ' ' ) {
					p++;
				}
				*p = '\0';
				state->recv_state = MOVED;
				DBG ( "HTTP got redirect to %s\n",
				      state->url );
				return 1;
			}
			/* Find beginning of line */
			while (length > 0) {
				length--;
				if (*((const char *)buffer)++ == '\n')
					break;
			}
			/* Check for end of header */
			if (length >= 2 && !memcmp(buffer, "\r\n", 2)) {
				state->recv_state = DATA;
				buffer += 2;
				length -= 2;
				break;
			}
		}
	}
	if (state->recv_state == DATA) {
		state->bytes_received += length;
		DBG2 ( "HTTP received %d bytes\n", length );
		while (length > 0) {
			int copy_length = BLOCKSIZE - state->recv_length;
			if (copy_length > length)
				copy_length = length;
			memcpy(state->recv_buffer + state->recv_length,
			       buffer, copy_length);
			if ((state->recv_length += copy_length) == BLOCKSIZE) {
				DBG2 ( "HTTP processing %d bytes\n",
				      BLOCKSIZE );
				if (!state->process(state->recv_buffer,
						    ++state->block,
						    BLOCKSIZE, 0))
				state->recv_length = 0;
			}
			length -= copy_length;
			buffer += copy_length;
		}
	}
	return 1;
}

/**************************************************************************
HTTP_GET - Get data using HTTP
**************************************************************************/
static int http ( char *url,
		  struct sockaddr_in *server __unused,
		  char *file __unused,
		  int ( * process ) ( unsigned char *data,
				      unsigned int blocknum,
				      unsigned int len, int eof ) ) {
	struct protocol *proto;
	struct sockaddr_in http_server = *server;
	char *filename;
	static const char GET[] = "GET /%s HTTP/1.0\r\n\r\n";
	static char recv_buffer[BLOCKSIZE];
	struct send_recv_state state;
	int length;

	state.rc = -1;
	state.block = 0;
	state.recv_buffer = recv_buffer;
	state.url = url;
	state.process = process;
	while ( 1 ) {
		length = strlen ( filename ) + strlen ( GET );
		{
			char send_buf[length];

			sprintf ( send_buf, GET, filename );
			state.send_buffer = send_buf;
			state.send_length = strlen ( send_buf );
			state.bytes_sent = 0;
			
			state.bytes_received = 0;
			state.recv_state = RESULT_CODE;
			
			state.recv_length = 0;
			tcp_transaction ( server->sin_addr.s_addr,
					  server->sin_port, &state,
					  send_tcp_request, recv_tcp_request );
		}

		if ( state.recv_state == MOVED ) {
			if ( ! parse_url ( state.url, &proto,
					   &http_server, &filename ) ) {
				printf ( "Invalid redirect URL %s\n",
					 state.url );
				return 0;
			}
			continue;
		}
		
		break;
	}

	if ( state.rc == 200 ) {
		DBG2 ( "HTTP processing %d bytes\n", state.recv_length );
		return process ( recv_buffer, ++state.block,
				 state.recv_length, 1 );
	} else {
		printf ( "Failed to download %s (rc = %d)\n",
			 state.url, state.rc );
		return 0;
	}
}

static struct protocol http_protocol __protocol = {
	.name = "http",
	.default_port = 80,
	.load = http,
};
