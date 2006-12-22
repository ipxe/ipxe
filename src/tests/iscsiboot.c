#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <vsprintf.h>
#include <gpxe/netdevice.h>
#include <gpxe/iscsi.h>
#include <gpxe/ibft.h>
#include <int13.h>

static struct iscsi_device test_iscsidev;

int test_iscsiboot ( const char *initiator_iqn,
		     struct sockaddr_tcpip *target,
		     const char *target_iqn,
		     unsigned int lun,
		     const char *username,
		     const char *password,
		     struct net_device *netdev,
		     unsigned int drivenum ) {
	struct int13_drive drive;
	int rc;

	memset ( &test_iscsidev, 0, sizeof ( test_iscsidev ) );
	memcpy ( &test_iscsidev.iscsi.target, target,
		 sizeof ( test_iscsidev.iscsi.target ) );
	test_iscsidev.iscsi.initiator_iqn = initiator_iqn;
	test_iscsidev.iscsi.target_iqn = target_iqn;
	test_iscsidev.iscsi.lun = lun;
	test_iscsidev.iscsi.username = username;
	test_iscsidev.iscsi.password = password;

	printf ( "Initialising %s\n", target_iqn );
	if ( ( rc = init_iscsidev ( &test_iscsidev ) ) != 0 ) {
		printf ( "Could not reach %s: %s\n", target_iqn,
			 strerror ( rc ) );
		return rc;
	}
	ibft_fill_data ( netdev, &test_iscsidev.iscsi );
	memset ( &drive, 0, sizeof ( drive ) );
	drive.drive = drivenum;
	drive.blockdev = &test_iscsidev.scsi.blockdev;
	register_int13_drive ( &drive );
	printf ( "Registered %s as BIOS drive %#02x\n",
		 target_iqn, drive.drive );
	printf ( "Booting from BIOS drive %#02x\n", drive.drive );
	rc = int13_boot ( drive.drive );
	printf ( "Boot failed\n" );

	printf ( "Unregistering BIOS drive %#02x\n", drive.drive );
	unregister_int13_drive ( &drive );

	fini_iscsidev ( &test_iscsidev );

	return rc;
}
