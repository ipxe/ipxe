/*
 * Copyright (C) 2011 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include <ipxe/init.h>
#include <ipxe/pci.h>
#include <ipxe/ethernet.h>
#include <ipxe/bofm.h>

/** @file
 *
 * IBM BladeCenter Open Fabric Manager (BOFM) tests
 *
 */

/** Harvest test table */
static struct {
	struct bofm_global_header header;
	struct bofm_section_header en_header;
	struct bofm_en en;
	struct bofm_section_header done;
} __attribute__ (( packed )) bofmtab_harvest = {
	.header = {
		.magic = BOFM_IOAA_MAGIC,
		.action = BOFM_ACTION_HVST,
		.version = 0x01,
		.level = 0x01,
		.length = sizeof ( bofmtab_harvest ),
		.profile = "Harvest test profile",
	},
	.en_header = {
		.magic = BOFM_EN_MAGIC,
		.length = sizeof ( bofmtab_harvest.en ),
	},
	.en = {
		.options = ( BOFM_EN_MAP_PFA | BOFM_EN_USAGE_HARVEST |
			     BOFM_EN_RQ_HVST_ACTIVE ),
		.mport = 1,
	},
	.done = {
		.magic = BOFM_DONE_MAGIC,
	},
};

/** Update test table */
static struct {
	struct bofm_global_header header;
	struct bofm_section_header en_header;
	struct bofm_en en;
	struct bofm_section_header done;
} __attribute__ (( packed )) bofmtab_update = {
	.header = {
		.magic = BOFM_IOAA_MAGIC,
		.action = BOFM_ACTION_UPDT,
		.version = 0x01,
		.level = 0x01,
		.length = sizeof ( bofmtab_update ),
		.profile = "Update test profile",
	},
	.en_header = {
		.magic = BOFM_EN_MAGIC,
		.length = sizeof ( bofmtab_update.en ),
	},
	.en = {
		.options = ( BOFM_EN_MAP_PFA | BOFM_EN_EN_A |
			     BOFM_EN_USAGE_ENTRY ),
		.mport = 1,
		.mac_a = { 0x02, 0x00, 0x69, 0x50, 0x58, 0x45 },
	},
	.done = {
		.magic = BOFM_DONE_MAGIC,
	},
};

/**
 * Perform BOFM test
 *
 * @v pci		PCI device
 */
void bofm_test ( struct pci_device *pci ) {
	int bofmrc;

	printf ( "BOFMTEST using " PCI_FMT "\n", PCI_ARGS ( pci ) );

	/* Perform harvest test */
	printf ( "BOFMTEST performing harvest\n" );
	bofmtab_harvest.en.busdevfn = pci->busdevfn;
	DBG_HDA ( 0, &bofmtab_harvest, sizeof ( bofmtab_harvest ) );
	bofmrc = bofm ( &bofmtab_harvest, pci );
	printf ( "BOFMTEST harvest result %08x\n", bofmrc );
	if ( bofmtab_harvest.en.options & BOFM_EN_HVST ) {
		printf ( "BOFMTEST harvested MAC address %s\n",
			 eth_ntoa ( &bofmtab_harvest.en.mac_a ) );
	} else {
		printf ( "BOFMTEST failed to harvest a MAC address\n" );
	}
	DBG_HDA ( 0, &bofmtab_harvest, sizeof ( bofmtab_harvest ) );

	/* Perform update test */
	printf ( "BOFMTEST performing update\n" );
	bofmtab_update.en.busdevfn = pci->busdevfn;
	DBG_HDA ( 0, &bofmtab_update, sizeof ( bofmtab_update ) );
	bofmrc = bofm ( &bofmtab_update, pci );
	printf ( "BOFMTEST update result %08x\n", bofmrc );
	if ( bofmtab_update.en.options & BOFM_EN_CSM_SUCCESS ) {
		printf ( "BOFMTEST updated MAC address to %s\n",
			 eth_ntoa ( &bofmtab_update.en.mac_a ) );
	} else {
		printf ( "BOFMTEST failed to update MAC address\n" );
	}
	DBG_HDA ( 0, &bofmtab_update, sizeof ( bofmtab_update ) );
}

/**
 * Harvest dummy Ethernet MAC
 *
 * @v bofm		BOFM device
 * @v mport		Multi-port index
 * @v mac		MAC to fill in
 * @ret rc		Return status code
 */
static int bofm_dummy_harvest ( struct bofm_device *bofm, unsigned int mport,
				uint8_t *mac ) {
	struct {
		uint16_t vendor;
		uint16_t device;
		uint16_t mport;
	} __attribute__ (( packed )) dummy_mac;

	/* Construct dummy MAC address */
	dummy_mac.vendor = cpu_to_be16 ( bofm->pci->vendor );
	dummy_mac.device = cpu_to_be16 ( bofm->pci->device );
	dummy_mac.mport = cpu_to_be16 ( mport );
	memcpy ( mac, &dummy_mac, sizeof ( dummy_mac ) );
	printf ( "BOFMTEST mport %d constructed dummy MAC %s\n",
		 mport, eth_ntoa ( mac ) );

	return 0;
}

/**
 * Update Ethernet MAC for BOFM
 *
 * @v bofm		BOFM device
 * @v mport		Multi-port index
 * @v mac		MAC to fill in
 * @ret rc		Return status code
 */
static int bofm_dummy_update ( struct bofm_device *bofm __unused,
			       unsigned int mport, const uint8_t *mac ) {

	printf ( "BOFMTEST mport %d asked to update MAC to %s\n",
		 mport, eth_ntoa ( mac ) );
	return 0;
}

/** Dummy BOFM operations */
static struct bofm_operations bofm_dummy_operations = {
	.harvest = bofm_dummy_harvest,
	.update = bofm_dummy_update,
};

/** Dummy BOFM device */
static struct bofm_device bofm_dummy;

/**
 * Probe dummy BOFM device
 *
 * @v pci		PCI device
 * @v id		PCI ID
 * @ret rc		Return status code
 */
static int bofm_dummy_probe ( struct pci_device *pci ) {
	int rc;

	/* Ignore probe for any other devices */
	if ( pci->busdevfn != bofm_dummy.pci->busdevfn )
		return 0;

	/* Register BOFM device */
	if ( ( rc = bofm_register ( &bofm_dummy ) ) != 0 )
		return rc;

	printf ( "BOFMTEST using dummy BOFM driver\n" );
	return 0;
}

/**
 * Remove dummy BOFM device
 *
 * @v pci		PCI device
 */
static void bofm_dummy_remove ( struct pci_device *pci ) {

	/* Ignore removal for any other devices */
	if ( pci->busdevfn != bofm_dummy.pci->busdevfn )
		return;

	/* Unregister BOFM device */
	bofm_unregister ( &bofm_dummy );
}

/** Dummy BOFM driver PCI IDs */
static struct pci_device_id bofm_dummy_ids[1] = {
	{ .name = "dummy" },
};

/** Dummy BOFM driver */
struct pci_driver bofm_dummy_driver __bofm_test_driver = {
	.ids = bofm_dummy_ids,
	.id_count = ( sizeof ( bofm_dummy_ids ) /
		      sizeof ( bofm_dummy_ids[0] ) ),
	.probe = bofm_dummy_probe,
	.remove = bofm_dummy_remove,
};

/**
 * Perform BOFM test at initialisation time
 *
 */
static void bofm_test_init ( void ) {
	struct pci_device pci;
	int busdevfn = -1;
	int rc;

	/* Uncomment the following line and specify the correct PCI
	 * bus:dev.fn address in order to perform a BOFM test at
	 * initialisation time.
	 */
	// busdevfn = PCI_BUSDEVFN ( <segment>, <bus>, <dev>, <fn> );

	/* Skip test if no PCI bus:dev.fn is defined */
	if ( busdevfn < 0 )
		return;

	/* Initialise PCI device */
	memset ( &pci, 0, sizeof ( pci ) );
	pci_init ( &pci, busdevfn );
	if ( ( rc = pci_read_config ( &pci ) ) != 0 ) {
		printf ( "BOFMTEST could not create " PCI_FMT " device: %s\n",
			 PCI_ARGS ( &pci ), strerror ( rc ) );
		return;
	}

	/* Initialise dummy BOFM device */
	bofm_init ( &bofm_dummy, &pci, &bofm_dummy_operations );
	bofm_dummy_ids[0].vendor = pci.vendor;
	bofm_dummy_ids[0].device = pci.device;

	/* Perform test */
	bofm_test ( &pci );
}

/** BOFM test initialisation function */
struct init_fn bofm_test_init_fn __init_fn ( INIT_NORMAL ) = {
	.initialise = bofm_test_init,
};
