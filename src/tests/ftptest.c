#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <console.h>
#include <vsprintf.h>
#include <gpxe/async.h>
#include <gpxe/buffer.h>
#include <gpxe/ftp.h>

static void print_ftp_response ( char *data, size_t len ) {
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
	char data[256];
	struct buffer buffer;
	struct ftp_request ftp;
	int rc;

	printf ( "FTP fetching %s\n", filename );
	
	memset ( &buffer, 0, sizeof ( buffer ) );
	buffer.addr = virt_to_phys ( data );
	buffer.len = sizeof ( data );

	memset ( &ftp, 0, sizeof ( ftp ) );
	memcpy ( &ftp.server, server, sizeof ( ftp.server ) );
	ftp.filename = filename;
	ftp.buffer = &buffer;

	rc = async_wait ( ftp_get ( &ftp ) );
	if ( rc ) {
		printf ( "FTP fetch failed\n" );
		return;
	}

	printf ( "FTP received %d bytes\n", buffer.fill );

	print_ftp_response ( data, buffer.fill );
}
