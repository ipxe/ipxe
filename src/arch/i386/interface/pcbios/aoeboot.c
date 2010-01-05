#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <gpxe/aoe.h>
#include <gpxe/ata.h>
#include <gpxe/netdevice.h>
#include <gpxe/sanboot.h>
#include <gpxe/abft.h>
#include <int13.h>

FILE_LICENCE ( GPL2_OR_LATER );

static int aoeboot ( const char *root_path ) {
	struct ata_device *ata;
	struct int13_drive *drive;
	int rc;

	ata = zalloc ( sizeof ( *ata ) );
	if ( ! ata ) {
		rc = -ENOMEM;
		goto err_alloc_ata;
	}
	drive = zalloc ( sizeof ( *drive ) );
	if ( ! drive ) {
		rc = -ENOMEM;
		goto err_alloc_drive;
	}

	/* FIXME: ugly, ugly hack */
	struct net_device *netdev = last_opened_netdev();

	if ( ( rc = aoe_attach ( ata, netdev, root_path ) ) != 0 ) {
		printf ( "Could not attach AoE device: %s\n",
			 strerror ( rc ) );
		goto err_attach;
	}
	if ( ( rc = init_atadev ( ata ) ) != 0 ) {
		printf ( "Could not initialise AoE device: %s\n",
			 strerror ( rc ) );
		goto err_init;
	}

	/* FIXME: ugly, ugly hack */
	struct aoe_session *aoe =
		container_of ( ata->backend, struct aoe_session, refcnt );
	abft_fill_data ( aoe );

	drive->blockdev = &ata->blockdev;

	register_int13_drive ( drive );
	printf ( "Registered as BIOS drive %#02x\n", drive->drive );
	printf ( "Booting from BIOS drive %#02x\n", drive->drive );
	rc = int13_boot ( drive->drive );
	printf ( "Boot failed\n" );

	/* Leave drive registered, if instructed to do so */
	if ( keep_san() )
		return rc;

	printf ( "Unregistering BIOS drive %#02x\n", drive->drive );
	unregister_int13_drive ( drive );

 err_init:
	aoe_detach ( ata );
 err_attach:
	free ( drive );
 err_alloc_drive:
	free ( ata );
 err_alloc_ata:
	return rc;
}

struct sanboot_protocol aoe_sanboot_protocol __sanboot_protocol = {
	.prefix = "aoe:",
	.boot = aoeboot,
};
