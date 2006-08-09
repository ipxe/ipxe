#include <stdint.h>
#include <string.h>
#include <console.h>
#include <gpxe/udp.h>
#include <gpxe/tftp.h>
#include <gpxe/async.h>

static void test_tftp_callback ( struct tftp_session *tftp __unused,
				 unsigned int block __unused,
				 void *data, size_t len ) {
	unsigned int i;
	char c;

	for ( i = 0 ; i < len ; i++ ) {
		c = * ( ( char * ) data + i );
		if ( c == '\r' ) {
			/* Print nothing */
		} else if ( ( c == '\n' ) || ( c >= 32 ) || ( c <= 126 ) ) {
			putchar ( c );
		} else {
			putchar ( '.' );
		}
	}	
}

int test_tftp ( struct sockaddr_tcpip *target, const char *filename ) {
	struct tftp_session tftp;

	memset ( &tftp, 0, sizeof ( tftp ) );
	udp_connect ( &tftp.udp, target );
	tftp.filename = filename;
	tftp.callback = test_tftp_callback;

	return async_wait ( tftp_get ( &tftp ) );
}
