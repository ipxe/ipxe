#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <console.h>
#include <vsprintf.h>
#include <gpxe/async.h>
#include <gpxe/ftp.h>

static void test_ftp_callback ( char *data, size_t len ) {
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

void test_ftp ( struct sockaddr_tcpip *server, const char *filename ) {
	struct ftp_request ftp;
	int rc;

	printf ( "FTP fetching %s\n", filename );
	
	memset ( &ftp, 0, sizeof ( ftp ) );
	memcpy ( &ftp.tcp.peer, server, sizeof ( ftp.tcp.peer ) );
	ftp.filename = filename;
	ftp.callback = test_ftp_callback;

	rc = async_wait ( ftp_get ( &ftp ) );
	if ( rc ) {
		printf ( "FTP fetch failed\n" );
	}
}
