#include <stdint.h>
#include <vsprintf.h>
#include <console.h>
#include <gpxe/netdevice.h>
#include <gpxe/aoe.h>
#include <int13.h>

static struct aoe_device test_aoedev = {
	.aoe = {
		.major = 0,
		.minor = 0,
	},
};

int test_aoeboot ( struct net_device *netdev ) {
	struct int13_drive drive;
	int rc;

	test_aoedev.aoe.netdev = netdev;
	printf ( "Initialising AoE device e%d.%d\n",
		 test_aoedev.aoe.major, test_aoedev.aoe.minor );
	if ( ( rc = init_aoedev ( &test_aoedev ) ) != 0 ) {
		printf ( "Could not reach AoE device e%d.%d\n",
			 test_aoedev.aoe.major, test_aoedev.aoe.minor );
		return rc;
	}

	memset ( &drive, 0, sizeof ( drive ) );
	drive.blockdev = &test_aoedev.ata.blockdev;
	register_int13_drive ( &drive );
	printf ( "Registered AoE device e%d.%d as BIOS drive %#02x\n",
		 test_aoedev.aoe.major, test_aoedev.aoe.minor, drive.drive );

	printf ( "Booting from BIOS drive %#02x\n", drive.drive );
	rc = int13_boot ( drive.drive );
	printf ( "Boot failed\n" );

	printf ( "Unregistering BIOS drive %#02x\n", drive.drive );
	unregister_int13_drive ( &drive );

	return rc;
}
