#include "etherboot.h"
#include "http.h"

#ifdef DOWNLOAD_PROTO_HTTP

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
	int (*fnc)(unsigned char *data, int block, int len, int eof);
	char *send_buffer;
	char *recv_buffer;
	int send_length;
	int recv_length;
	int bytes_sent;
	int block;
	int bytes_received;
	enum { RESULT_CODE, HEADER, DATA, ERROR, MOVED } recv_state;
	int rc;
	char location[MAX_URL+1];
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
					return 0;
				}
				state->rc = rc;
				state->recv_state = HEADER;
				goto header;
			}
			++(const char *)buffer;
			length--;
		}
		state->recv_state = ERROR;
		return 0;
	}
	if (state->recv_state == HEADER) {
	header: while (length > 0) {
			/* Check for HTTP redirect */
			if (state->rc >= 300 && state->rc < 400 &&
			    !memcmp(buffer, "Location: ", 10)) {
				char *ptr = state->location;
				int i;
				memcpy(ptr, buffer + 10, MAX_URL);
				for (i = 0; i < MAX_URL && *ptr > ' ';
				     i++, ptr++);
				*ptr = '\000';
				state->recv_state = MOVED;
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
		while (length > 0) {
			int copy_length = BLOCKSIZE - state->recv_length;
			if (copy_length > length)
				copy_length = length;
			memcpy(state->recv_buffer + state->recv_length,
			       buffer, copy_length);
			if ((state->recv_length += copy_length) == BLOCKSIZE) {
				if (!state->fnc(state->recv_buffer,
						++state->block, BLOCKSIZE, 0))
					return 0;
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
int http(const char *url,
		int (*fnc)(unsigned char *, unsigned int, unsigned int, int)) {
	static const char GET[] = "GET /%s HTTP/1.0\r\n\r\n";
	static char recv_buffer[BLOCKSIZE];
	in_addr destip;
	int port;
	int length;
	struct send_recv_state state;

	state.fnc = fnc;
	state.rc = -1;
	state.block = 0;
	state.recv_buffer = recv_buffer;
	length = strlen(url);
	if (length <= MAX_URL) {
		memcpy(state.location, url, length+1);
		destip = arptable[ARP_SERVER].ipaddr;
		port = url_port;
		if (port == -1)
		  port = 80;
		goto first_time;

		do {
			state.rc = -1;
			state.block = 0;
			url = state.location;
			if (memcmp("http://", url, 7))
				break;
			url += 7;
			length = inet_aton(url, &destip);
			if (!length) {
				/* As we do not have support for DNS, assume*/
				/* that HTTP redirects always point to the */
				/* same machine */
				if (state.recv_state == MOVED) {
					while (*url &&
					*url != ':' && *url != '/') url++;
				} else {
					break;
				}
			}
			if (*(url += length) == ':') {
				port = strtoul(url, &url, 10);
			} else {
				port = 80;
			}
			if (!*url)
				url = "/";
			if (*url != '/')
				break;
			url++;

		first_time:
			length = strlen(url);
			state.send_length = sizeof(GET) - 3 + length;
			
			{ char buf[state.send_length + 1];
			sprintf(state.send_buffer = buf, GET, url);
			state.bytes_sent = 0;
			
			state.bytes_received = 0;
			state.recv_state = RESULT_CODE;
			
			state.recv_length = 0;
			tcp_transaction(destip.s_addr, 80, &state,
					send_tcp_request, recv_tcp_request);
			}
		} while (state.recv_state == MOVED);
	} else {
		memcpy(state.location, url, MAX_URL);
		state.location[MAX_URL] = '\000';
	}

	if (state.rc == 200) {
		return fnc(recv_buffer, ++state.block, state.recv_length, 1);
	} else {
		printf("Failed to download %s (rc = %d)\n",
		       state.location, state.rc);
		return 0;
	}
}

#endif /* DOWNLOAD_PROTO_HTTP */
