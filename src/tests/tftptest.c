#include <stdint.h>
#include <string.h>
#include <console.h>
#include <gpxe/udp.h>
#include <gpxe/tftp.h>
#include <gpxe/async.h>
#include <gpxe/uaccess.h>
#include <gpxe/buffer.h>
#include "pxe.h"

int test_tftp ( struct net_device *netdev, struct sockaddr_tcpip *target,
		const char *filename ) {
	struct tftp_session tftp;
	struct buffer buffer;
	int rc;

	memset ( &buffer, 0, sizeof ( buffer ) );
	buffer.addr = real_to_user ( 0, 0x7c00 );
	buffer.len = ( 512 * 1024 - 0x7c00 );

	memset ( &tftp, 0, sizeof ( tftp ) );
	udp_connect ( &tftp.udp, target );
	tftp.filename = filename;
	tftp.buffer = &buffer;

	printf ( "Fetching \"%s\" via TFTP\n", filename );
	if ( ( rc = async_wait ( tftp_get ( &tftp ) ) ) != 0 )
		return rc;

	printf ( "Attempting PXE boot\n" );
	pxe_netdev = netdev;
	rc = pxe_boot();
	printf ( "PXE NBP returned with status %04x\n", rc );
	return 0;
}
