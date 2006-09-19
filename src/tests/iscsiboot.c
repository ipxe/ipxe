#include <stdint.h>
#include <byteswap.h>
#include <vsprintf.h>
#include <gpxe/netdevice.h>
#include <gpxe/iscsi.h>
#if 0
#include <gpxe/ibft.h>
#endif
#include <int13.h>

static struct iscsi_device test_iscsidev;

int test_iscsiboot ( const char *initiator_iqn,
		     struct sockaddr_tcpip *target,
		     const char *target_iqn,
		     struct net_device *netdev ) {
	struct int13_drive drive;
	int rc;

	memset ( &test_iscsidev, 0, sizeof ( test_iscsidev ) );
	memcpy ( &test_iscsidev.iscsi.tcp.peer, target,
		 sizeof ( test_iscsidev.iscsi.tcp.peer ) );
	test_iscsidev.iscsi.initiator = initiator_iqn;
	test_iscsidev.iscsi.target = target_iqn;

	printf ( "Initialising %s\n", target_iqn );
	if ( ( rc = init_iscsidev ( &test_iscsidev ) ) != 0 ) {
		printf ( "Could not reach %s\n", target_iqn );
		return rc;
	}
#if 0
	ibft_fill_data ( netdev, initiator_iqn, target, target_iqn );
#endif
	memset ( &drive, 0, sizeof ( drive ) );
	drive.blockdev = &test_iscsidev.scsi.blockdev;
	register_int13_drive ( &drive );
	printf ( "Registered %s as BIOS drive %#02x\n",
		 target_iqn, drive.drive );
	printf ( "Booting from BIOS drive %#02x\n", drive.drive );
	rc = int13_boot ( drive.drive );
	printf ( "Boot failed\n" );

	printf ( "Unregistering BIOS drive %#02x\n", drive.drive );
	unregister_int13_drive ( &drive );

	return rc;
}
