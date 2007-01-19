#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
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

static int aoe_parse ( const char *aoename, struct aoe_session *aoe ) {
	char *ptr = ( ( char * ) aoename );

	if ( *ptr++ != 'e' )
		return -EINVAL;

	aoe->major = strtoul ( ptr, &ptr, 10 );
	if ( *ptr++ != '.' )
		return -EINVAL;

	aoe->minor = strtoul ( ptr, &ptr, 10 );
	if ( *ptr )
		return -EINVAL;

	return 0;
}

int test_aoeboot ( struct net_device *netdev, const char *aoename,
		   unsigned int drivenum ) {
	struct int13_drive drive;
	int rc;

	printf ( "Attempting to boot from AoE device %s via %s\n",
		 aoename, netdev->name );

	if ( ( rc = aoe_parse ( aoename, &test_aoedev.aoe ) ) != 0 ) {
		printf ( "Invalid AoE device name \"%s\"\n", aoename );
		return rc;
	}

	printf ( "Initialising AoE device e%d.%d\n",
		 test_aoedev.aoe.major, test_aoedev.aoe.minor );
	test_aoedev.aoe.netdev = netdev;
	if ( ( rc = init_aoedev ( &test_aoedev ) ) != 0 ) {
		printf ( "Could not reach AoE device e%d.%d\n",
			 test_aoedev.aoe.major, test_aoedev.aoe.minor );
		return rc;
	}

	memset ( &drive, 0, sizeof ( drive ) );
	drive.drive = drivenum;
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
