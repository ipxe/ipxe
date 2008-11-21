#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <gpxe/iscsi.h>
#include <gpxe/settings.h>
#include <gpxe/dhcp.h>
#include <gpxe/netdevice.h>
#include <gpxe/ibft.h>
#include <gpxe/init.h>
#include <gpxe/sanboot.h>
#include <int13.h>
#include <usr/autoboot.h>

struct setting keep_san_setting __setting = {
	.name = "keep-san",
	.description = "Preserve SAN connection",
	.tag = DHCP_EB_KEEP_SAN,
	.type = &setting_type_int8,
};

static int iscsiboot ( const char *root_path ) {
	struct scsi_device *scsi;
	struct int13_drive *drive;
	int keep_san;
	int rc;

	scsi = zalloc ( sizeof ( *scsi ) );
	if ( ! scsi ) {
		rc = -ENOMEM;
		goto err_alloc_scsi;
	}
	drive = zalloc ( sizeof ( *drive ) );
	if ( ! drive ) {
		rc = -ENOMEM;
		goto err_alloc_drive;
	}

	printf ( "iSCSI booting from %s\n", root_path );

	if ( ( rc = iscsi_attach ( scsi, root_path ) ) != 0 ) {
		printf ( "Could not attach iSCSI device: %s\n",
			 strerror ( rc ) );
		goto err_attach;
	}
	if ( ( rc = init_scsidev ( scsi ) ) != 0 ) {
		printf ( "Could not initialise iSCSI device: %s\n",
			 strerror ( rc ) );
		goto err_init;
	}

	drive->blockdev = &scsi->blockdev;

	/* FIXME: ugly, ugly hack */
	struct net_device *netdev = last_opened_netdev();
	struct iscsi_session *iscsi =
		container_of ( scsi->backend, struct iscsi_session, refcnt );
	ibft_fill_data ( netdev, iscsi );

	register_int13_drive ( drive );
	printf ( "Registered as BIOS drive %#02x\n", drive->drive );
	printf ( "Booting from BIOS drive %#02x\n", drive->drive );
	rc = int13_boot ( drive->drive );
	printf ( "Boot failed\n" );

	/* Leave drive registered, if instructed to do so */
	keep_san = fetch_intz_setting ( NULL, &keep_san_setting );
	if ( keep_san ) {
		printf ( "Preserving connection to SAN disk\n" );
		shutdown_exit_flags |= SHUTDOWN_KEEP_DEVICES;
		return rc;
	}

	printf ( "Unregistering BIOS drive %#02x\n", drive->drive );
	unregister_int13_drive ( drive );

 err_init:
	iscsi_detach ( scsi );
 err_attach:
	free ( drive );
 err_alloc_drive:
	free ( scsi );
 err_alloc_scsi:
	return rc;
}

struct sanboot_protocol iscsi_sanboot_protocol __sanboot_protocol = {
	.prefix = "iscsi:",
	.boot = iscsiboot,
};
