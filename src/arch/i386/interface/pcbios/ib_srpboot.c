#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <gpxe/sanboot.h>
#include <int13.h>
#include <gpxe/srp.h>
#include <gpxe/sbft.h>

FILE_LICENCE ( GPL2_OR_LATER );

static int ib_srpboot ( const char *root_path ) {
	struct scsi_device *scsi;
	struct int13_drive *drive;
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

	if ( ( rc = srp_attach ( scsi, root_path ) ) != 0 ) {
		printf ( "Could not attach IB_SRP device: %s\n",
			 strerror ( rc ) );
		goto err_attach;
	}
	if ( ( rc = init_scsidev ( scsi ) ) != 0 ) {
		printf ( "Could not initialise IB_SRP device: %s\n",
			 strerror ( rc ) );
		goto err_init;
	}

	drive->blockdev = &scsi->blockdev;

	/* FIXME: ugly, ugly hack */
	struct srp_device *srp =
		container_of ( scsi->backend, struct srp_device, refcnt );
	sbft_fill_data ( srp );

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
	srp_detach ( scsi );
 err_attach:
	free ( drive );
 err_alloc_drive:
	free ( scsi );
 err_alloc_scsi:
	return rc;
}

struct sanboot_protocol ib_srp_sanboot_protocol __sanboot_protocol = {
	.prefix = "ib_srp:",
	.boot = ib_srpboot,
};
