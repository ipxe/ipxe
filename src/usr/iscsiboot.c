#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <gpxe/iscsi.h>
#include <gpxe/dhcp.h>
#include <int13.h>
#include <usr/iscsiboot.h>

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

	drive.drive = find_global_dhcp_num_option ( DHCP_EB_BIOS_DRIVE );
	drive.blockdev = &scsi.blockdev;

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
