/*
 * Copyright Fen Systems Ltd. 2007.  Portions of this code are derived
 * from IBM Corporation Sample Programs.  Copyright IBM Corporation
 * 2004, 2007.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <realmode.h>
#include <gpxe/pci.h>
#include <gpxe/acpi.h>
#include <gpxe/in.h>
#include <gpxe/netdevice.h>
#include <gpxe/dhcp.h>
#include <gpxe/iscsi.h>
#include <gpxe/ibft.h>

/** @file
 *
 * iSCSI boot firmware table
 *
 * The information in this file is derived from the document "iSCSI
 * Boot Firmware Table (iBFT)" as published by IBM at
 *
 * ftp://ftp.software.ibm.com/systems/support/system_x_pdf/ibm_iscsi_boot_firmware_table_v1.02.pdf
 *
 */

#define ibftab __use_data16 ( ibftab )
/** The iBFT used by gPXE */
struct gpxe_ibft __data16 ( ibftab ) = {
	/* Table header */
	.table = {
		/* ACPI header */
		.acpi = {
			.signature = IBFT_SIG,
			.length = sizeof ( ibftab ),
			.revision = 1,
			.oem_id = "FENSYS",
			.oem_table_id = "gPXE",
		},
		/* Control block */
		.control = {
			.header = {
				.structure_id = IBFT_STRUCTURE_ID_CONTROL,
				.version = 1,
				.length = sizeof ( ibftab.table.control ),
				.flags = 0,
			},
			.initiator = offsetof ( typeof ( ibftab ), initiator ),
			.nic_0 = offsetof ( typeof ( ibftab ), nic ),
			.target_0 = offsetof ( typeof ( ibftab ), target ),
		},
	},
	/* iSCSI initiator information */
	.initiator = {
		.header = {
			.structure_id = IBFT_STRUCTURE_ID_INITIATOR,
			.version = 1,
			.length = sizeof ( ibftab.initiator ),
			.flags = ( IBFT_FL_INITIATOR_BLOCK_VALID |
				   IBFT_FL_INITIATOR_FIRMWARE_BOOT_SELECTED ),
		},
	},
	/* NIC information */
	.nic = {
		.header = {
			.structure_id = IBFT_STRUCTURE_ID_NIC,
			.version = 1,
			.length = sizeof ( ibftab.nic ),
			.flags = ( IBFT_FL_NIC_BLOCK_VALID |
				   IBFT_FL_NIC_FIRMWARE_BOOT_SELECTED ),
		},
	},
	/* iSCSI target information */
	.target = {
		.header = {
			.structure_id = IBFT_STRUCTURE_ID_TARGET,
			.version = 1,
			.length = sizeof ( ibftab.target ),
			.flags = ( IBFT_FL_TARGET_BLOCK_VALID |
				   IBFT_FL_TARGET_FIRMWARE_BOOT_SELECTED ),
		},
	},
};

/**
 * Fill in an IP address field within iBFT
 *
 * @v ipaddr		IP address field
 * @v in		IPv4 address
 */
static void ibft_set_ipaddr ( struct ibft_ipaddr *ipaddr, struct in_addr in ) {
	memset ( ipaddr, 0, sizeof ( ipaddr ) );
	if ( in.s_addr ) {
		ipaddr->in = in;
		ipaddr->ones = 0xffff;
	}
}

/**
 * Fill in an IP address within iBFT from DHCP option
 *
 * @v ipaddr		IP address field
 * @v tag		DHCP option tag
 */
static void ibft_set_ipaddr_option ( struct ibft_ipaddr *ipaddr,
				     unsigned int tag ) {
	struct in_addr in = { 0 };
	fetch_ipv4_setting ( NULL, tag, &in );
	ibft_set_ipaddr ( ipaddr, in );
}

/**
 * Allocate a string within iBFT
 *
 * @v strings		iBFT string block descriptor
 * @v string		String field to fill in
 * @v len		Length of string to allocate (excluding NUL)
 * @ret rc		Return status code
 */
static int ibft_alloc_string ( struct ibft_string_block *strings,
			       struct ibft_string *string, size_t len ) {
	char *dest;
	unsigned int remaining;

	dest = ( ( ( char * ) strings->table ) + strings->offset );
	remaining = ( strings->table->acpi.length - strings->offset );
	if ( len >= remaining )
		return -ENOMEM;

	string->offset = strings->offset;
	string->length = len;
	strings->offset += ( len + 1 );
	return 0;
}

/**
 * Fill in a string field within iBFT
 *
 * @v strings		iBFT string block descriptor
 * @v string		String field
 * @v data		String to fill in
 * @ret rc		Return status code
 */
static int ibft_set_string ( struct ibft_string_block *strings,
			     struct ibft_string *string, const char *data ) {
	size_t len = strlen ( data );
	char *dest;
	int rc;

	if ( ( rc = ibft_alloc_string ( strings, string, len ) ) != 0 )
		return rc;
	dest = ( ( ( char * ) strings->table ) + string->offset );
	strcpy ( dest, data );

	return 0;
}

/**
 * Fill in a string field within iBFT from DHCP option
 *
 * @v strings		iBFT string block descriptor
 * @v string		String field
 * @v tag		DHCP option tag
 * @ret rc		Return status code
 */
static int ibft_set_string_option ( struct ibft_string_block *strings,
				    struct ibft_string *string,
				    unsigned int tag ) {
	int len;
	char *dest;
	int rc;

	len = fetch_setting_len ( NULL, tag );
	if ( len < 0 ) {
		string->offset = 0;
		string->length = 0;
		return 0;
	}

	if ( ( rc = ibft_alloc_string ( strings, string, len ) ) != 0 )
		return rc;
	dest = ( ( ( char * ) strings->table ) + string->offset );
	fetch_string_setting ( NULL, tag, dest, ( len + 1 ) );
	return 0;
}

/**
 * Fill in NIC portion of iBFT
 *
 * @v nic		NIC portion of iBFT
 * @v strings		iBFT string block descriptor
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int ibft_fill_nic ( struct ibft_nic *nic,
			   struct ibft_string_block *strings,
			   struct net_device *netdev ) {
	struct in_addr netmask_addr = { 0 };
	unsigned int netmask_count = 0;
	int rc;

	/* Extract values from DHCP configuration */
	ibft_set_ipaddr_option ( &nic->ip_address, DHCP_EB_YIADDR );
	ibft_set_ipaddr_option ( &nic->gateway, DHCP_ROUTERS );
	ibft_set_ipaddr_option ( &nic->dns[0], DHCP_DNS_SERVERS );
	if ( ( rc = ibft_set_string_option ( strings, &nic->hostname,
					     DHCP_HOST_NAME ) ) != 0 )
		return rc;

	/* Derive subnet mask prefix from subnet mask */
	fetch_ipv4_setting ( NULL, DHCP_SUBNET_MASK, &netmask_addr );
	while ( netmask_addr.s_addr ) {
		if ( netmask_addr.s_addr & 0x1 )
			netmask_count++;
		netmask_addr.s_addr >>= 1;
	}
	nic->subnet_mask_prefix = netmask_count;

	/* Extract values from net-device configuration */
	memcpy ( nic->mac_address, netdev->ll_addr,
		 sizeof ( nic->mac_address ) );
	nic->pci_bus_dev_func = netdev->dev->desc.location;

	return 0;
}

/**
 * Fill in Initiator portion of iBFT
 *
 * @v initiator		Initiator portion of iBFT
 * @v strings		iBFT string block descriptor
 * @ret rc		Return status code
 */
static int ibft_fill_initiator ( struct ibft_initiator *initiator,
				 struct ibft_string_block *strings ) {
	const char *initiator_iqn = iscsi_initiator_iqn();
	int rc;

	if ( ( rc = ibft_set_string ( strings, &initiator->initiator_name,
				      initiator_iqn ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Fill in Target portion of iBFT
 *
 * @v target		Target portion of iBFT
 * @v strings		iBFT string block descriptor
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int ibft_fill_target ( struct ibft_target *target,
			      struct ibft_string_block *strings,
			      struct iscsi_session *iscsi ) {
	struct sockaddr_in *sin_target =
		( struct sockaddr_in * ) &iscsi->target_sockaddr;
	int rc;

	/* Fill in Target values */
	ibft_set_ipaddr ( &target->ip_address, sin_target->sin_addr );
	target->socket = ntohs ( sin_target->sin_port );
	if ( ( rc = ibft_set_string ( strings, &target->target_name,
				      iscsi->target_iqn ) ) != 0 )
		return rc;
	if ( iscsi->username ) {
		if ( ( rc = ibft_set_string ( strings, &target->chap_name,
					      iscsi->username ) ) != 0 )
			return rc;
	}
	if ( iscsi->password ) {
		if ( ( rc = ibft_set_string ( strings, &target->chap_secret,
					      iscsi->password ) ) != 0 )
			return rc;
		target->chap_type = IBFT_CHAP_ONE_WAY;
	}

	return 0;
}

/**
 * Fill in all variable portions of iBFT
 *
 * @v netdev		Network device
 * @v initiator_iqn	Initiator IQN
 * @v st_target		Target socket address
 * @v target_iqn	Target IQN
 * @ret rc		Return status code
 *
 */
int ibft_fill_data ( struct net_device *netdev,
		     struct iscsi_session *iscsi ) {
	struct ibft_string_block strings = {
		.table = &ibftab.table,
		.offset = offsetof ( typeof ( ibftab ), strings ),
	};
	int rc;

	/* Fill in NIC, Initiator and Target portions */
	if ( ( rc = ibft_fill_nic ( &ibftab.nic, &strings, netdev ) ) != 0 )
		return rc;
	if ( ( rc = ibft_fill_initiator ( &ibftab.initiator,
					  &strings ) ) != 0 )
		return rc;
	if ( ( rc = ibft_fill_target ( &ibftab.target, &strings,
				       iscsi ) ) != 0 )
		return rc;

	/* Update checksum */
	acpi_fix_checksum ( &ibftab.table.acpi );

	return 0;
}
