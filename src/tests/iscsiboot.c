#include <stdint.h>
#include <byteswap.h>
#include <vsprintf.h>
#include <gpxe/netdevice.h>
#include <gpxe/iscsi.h>
#include <int13.h>

static struct iscsi_device test_iscsidev;

int test_iscsiboot ( const char *initiator_iqn,
		     struct in_addr target,
		     const char *target_iqn ) {
	struct int13_drive drive;
	int rc;

	memset ( &test_iscsidev, 0, sizeof ( test_iscsidev ) );
	test_iscsidev.iscsi.tcp.sin.sin_addr = target;
	test_iscsidev.iscsi.tcp.sin.sin_port = htons ( ISCSI_PORT );
	test_iscsidev.iscsi.initiator = initiator_iqn;
	test_iscsidev.iscsi.target = target_iqn;

	printf ( "Initialising %s\n", target_iqn );
	if ( ( rc = init_iscsidev ( &test_iscsidev ) ) != 0 ) {
		printf ( "Could not reach %s\n", target_iqn );
		return rc;
	}

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
