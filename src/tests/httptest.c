#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <console.h>
#include <vsprintf.h>
#include <gpxe/async.h>
#include <gpxe/http.h>
#include <gpxe/ip.h>
#include <gpxe/uaccess.h>
#include "pxe.h"

static void test_http_callback ( struct http_request *http, char *data, size_t len ) {
	userptr_t pxe_buffer = real_to_user ( 0, 0x7c00 );
	unsigned long offset = http->file_recv;
	http->file_recv += len;
	copy_to_user ( pxe_buffer, offset, data, len );
}

void test_http ( struct net_device *netdev, struct sockaddr_tcpip *server, const char *filename ) {
	struct http_request http;
	int rc;

	memset ( &http, 0, sizeof ( http ) );
	memcpy ( &http.tcp.peer, server, sizeof ( http.tcp.peer ) );
	http.filename = filename;
	http.callback = test_http_callback;

	rc = async_wait ( get_http ( &http ) );
	if ( rc ) {
		printf ( "HTTP fetch failed\n" );
	}

	printf ( "Attempting PXE boot\n" );
	pxe_netdev = netdev;
	rc = pxe_boot();
	printf ( "PXE NBP returned with status %04x\n", rc);
}
