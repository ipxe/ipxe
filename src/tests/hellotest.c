#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <console.h>
#include <vsprintf.h>
#include <gpxe/async.h>
#include <gpxe/hello.h>

static void test_hello_callback ( char *data, size_t len ) {
	unsigned int i;
	char c;

	for ( i = 0 ; i < len ; i++ ) {
		c = data[i];
		if ( c == '\r' ) {
			/* Print nothing */
		} else if ( ( c == '\n' ) || ( c >= 32 ) || ( c <= 126 ) ) {
			putchar ( c );
		} else {
			putchar ( '.' );
		}
	}
}

void test_hello ( struct sockaddr_tcpip *server, const char *message ) {
	/* Quick and dirty hack */
	struct sockaddr_in *sin = ( struct sockaddr_in * ) server;
	struct hello_request hello;
	int rc;

	printf ( "Saying \"%s\" to %s:%d\n", message,
		 inet_ntoa ( sin->sin_addr ), ntohs ( sin->sin_port ) );
	
	memset ( &hello, 0, sizeof ( hello ) );
	memcpy ( &hello.server, server, sizeof ( hello.server ) );
	hello.message = message;
	hello.callback = test_hello_callback;

	rc = async_wait ( say_hello ( &hello ) );
	if ( rc ) {
		printf ( "HELLO fetch failed\n" );
	}
}
