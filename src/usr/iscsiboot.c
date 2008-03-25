#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <gpxe/iscsi.h>
#include <gpxe/settings.h>
#include <gpxe/netdevice.h>
#include <gpxe/ibft.h>
#include <int13.h>
#include <usr/iscsiboot.h>

/**
 * Guess boot network device
 *
 * @ret netdev		Boot network device
 */
static struct net_device * guess_boot_netdev ( void ) {
	struct net_device *boot_netdev;

	/* Just use the first network device */
	for_each_netdev ( boot_netdev ) {
		return boot_netdev;
	}

	return NULL;
}

int iscsiboot ( const char *root_path ) {
	struct scsi_device scsi;
	struct int13_drive drive;
	int rc;

	memset ( &scsi, 0, sizeof ( scsi ) );
	memset ( &drive, 0, sizeof ( drive ) );

	printf ( "iSCSI booting from %s\n", root_path );

	if ( ( rc = iscsi_attach ( &scsi, root_path ) ) != 0 ) {
		printf ( "Could not attach iSCSI device: %s\n",
			 strerror ( rc ) );
		goto error_attach;
	}
	if ( ( rc = init_scsidev ( &scsi ) ) != 0 ) {
		printf ( "Could not initialise iSCSI device: %s\n",
			 strerror ( rc ) );
		goto error_init;
	}

	drive.blockdev = &scsi.blockdev;

	/* FIXME: ugly, ugly hack */
	struct net_device *netdev = guess_boot_netdev();
	struct iscsi_session *iscsi =
		container_of ( scsi.backend, struct iscsi_session, refcnt );
	ibft_fill_data ( netdev, iscsi );

	register_int13_drive ( &drive );
	printf ( "Registered as BIOS drive %#02x\n", drive.drive );
	printf ( "Booting from BIOS drive %#02x\n", drive.drive );
	rc = int13_boot ( drive.drive );
	printf ( "Boot failed\n" );

	printf ( "Unregistering BIOS drive %#02x\n", drive.drive );
	unregister_int13_drive ( &drive );

 error_init:
	iscsi_detach ( &scsi );
 error_attach:
	return rc;
}
