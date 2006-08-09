#include <stdint.h>
#include <string.h>
#include <console.h>
#include <gpxe/udp.h>
#include <gpxe/tftp.h>
#include <gpxe/async.h>
#include <gpxe/uaccess.h>
#include "pxe.h"

static void test_tftp_callback ( struct tftp_session *tftp, unsigned int block,
				 void *data, size_t len ) {
	unsigned long offset = ( ( block - 1 ) * tftp->blksize );
	userptr_t pxe_buffer = real_to_user ( 0, 0x7c00 );

	copy_to_user ( pxe_buffer, offset, data, len );
}

int test_tftp ( struct net_device *netdev, struct sockaddr_tcpip *target,
		const char *filename ) {
	struct tftp_session tftp;
	int rc;

	memset ( &tftp, 0, sizeof ( tftp ) );
	udp_connect ( &tftp.udp, target );
	tftp.filename = filename;
	tftp.callback = test_tftp_callback;

	printf ( "Fetching \"%s\" via TFTP\n", filename );
	if ( ( rc = async_wait ( tftp_get ( &tftp ) ) ) != 0 )
		return rc;

	printf ( "Attempting PXE boot\n" );
	pxe_netdev = netdev;
	rc = pxe_boot();
	printf ( "PXE NBP returned with status %04x\n", rc );
	return 0;
}
