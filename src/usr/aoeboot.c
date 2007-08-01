#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <byteswap.h>
#include <gpxe/aoe.h>
#include <gpxe/ata.h>
#include <gpxe/netdevice.h>
#include <gpxe/dhcp.h>
#include <gpxe/abft.h>
#include <int13.h>
#include <usr/aoeboot.h>

/**
 * AoE boot information block
 *
 * Must be placed at 40:f0.
 *
 * This structure needs to be replaced by an ACPI table or similar.
 */
struct aoe_boot_info {
	/** Must be 0x01 */
	uint8_t one;
	/** Client MAC address */
	uint8_t client[ETH_ALEN];
	/** Server MAC address */
	uint8_t server[ETH_ALEN];
	/** Shelf number */
	uint16_t shelf;
	/** Slot number */
	uint8_t slot;
} __attribute__ (( packed ));

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

int aoeboot ( const char *root_path ) {
	struct ata_device ata;
	struct int13_drive drive;
	int rc;

	memset ( &ata, 0, sizeof ( ata ) );
	memset ( &drive, 0, sizeof ( drive ) );

	printf ( "AoE booting from %s\n", root_path );

	/* FIXME: ugly, ugly hack */
	struct net_device *netdev = guess_boot_netdev();

	if ( ( rc = aoe_attach ( &ata, netdev, root_path ) ) != 0 ) {
		printf ( "Could not attach AoE device: %s\n",
			 strerror ( rc ) );
		goto error_attach;
	}
	if ( ( rc = init_atadev ( &ata ) ) != 0 ) {
		printf ( "Could not initialise AoE device: %s\n",
			 strerror ( rc ) );
		goto error_init;
	}

	/* FIXME: ugly, ugly hack */
	struct aoe_session *aoe =
		container_of ( ata.backend, struct aoe_session, refcnt );
	struct aoe_boot_info boot_info;
	boot_info.one = 0x01;
	memcpy ( boot_info.client, netdev->ll_addr,
		 sizeof ( boot_info.client ) );
	memcpy ( boot_info.server, aoe->target,
		 sizeof ( boot_info.server ) );
	boot_info.shelf = htons ( aoe->major );
	boot_info.slot = aoe->minor;
	copy_to_real ( 0x40, 0xf0, &boot_info, sizeof ( boot_info ) );

	abft_fill_data ( aoe );

	drive.drive = find_global_dhcp_num_option ( DHCP_EB_BIOS_DRIVE );
	drive.blockdev = &ata.blockdev;

	register_int13_drive ( &drive );
	printf ( "Registered as BIOS drive %#02x\n", drive.drive );
	printf ( "Booting from BIOS drive %#02x\n", drive.drive );
	rc = int13_boot ( drive.drive );
	printf ( "Boot failed\n" );

	printf ( "Unregistering BIOS drive %#02x\n", drive.drive );
	unregister_int13_drive ( &drive );

 error_init:
	aoe_detach ( &ata );
 error_attach:
	return rc;
}
