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

FILE_LICENCE ( BSD2 );

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
#include <gpxe/ethernet.h>
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
 * Fill in an IP address within iBFT from configuration setting
 *
 * @v ipaddr		IP address field
 * @v setting		Configuration setting
 * @v tag		DHCP option tag
 */
static void ibft_set_ipaddr_option ( struct ibft_ipaddr *ipaddr,
				     struct setting *setting ) {
	struct in_addr in = { 0 };
	fetch_ipv4_setting ( NULL, setting, &in );
	ibft_set_ipaddr ( ipaddr, in );
}

/**
 * Read IP address from iBFT (for debugging)
 *
 * @v strings		iBFT string block descriptor
 * @v string		String field
 * @ret ipaddr		IP address string
 */
static const char * ibft_ipaddr ( struct ibft_ipaddr *ipaddr ) {
	return inet_ntoa ( ipaddr->in );
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
 * @v data		String to fill in, or NULL
 * @ret rc		Return status code
 */
static int ibft_set_string ( struct ibft_string_block *strings,
			     struct ibft_string *string, const char *data ) {
	char *dest;
	int rc;

	if ( ! data )
		return 0;

	if ( ( rc = ibft_alloc_string ( strings, string,
					strlen ( data ) ) ) != 0 )
		return rc;
	dest = ( ( ( char * ) strings->table ) + string->offset );
	strcpy ( dest, data );

	return 0;
}

/**
 * Fill in a string field within iBFT from configuration setting
 *
 * @v strings		iBFT string block descriptor
 * @v string		String field
 * @v setting		Configuration setting
 * @ret rc		Return status code
 */
static int ibft_set_string_option ( struct ibft_string_block *strings,
				    struct ibft_string *string,
				    struct setting *setting ) {
	int len;
	char *dest;
	int rc;

	len = fetch_setting_len ( NULL, setting );
	if ( len < 0 ) {
		string->offset = 0;
		string->length = 0;
		return 0;
	}

	if ( ( rc = ibft_alloc_string ( strings, string, len ) ) != 0 )
		return rc;
	dest = ( ( ( char * ) strings->table ) + string->offset );
	fetch_string_setting ( NULL, setting, dest, ( len + 1 ) );
	return 0;
}

/**
 * Read string from iBFT (for debugging)
 *
 * @v strings		iBFT string block descriptor
 * @v string		String field
 * @ret data		String content (or "<empty>")
 */
static const char * ibft_string ( struct ibft_string_block *strings,
				  struct ibft_string *string ) {
	return ( string->offset ?
		 ( ( ( char * ) strings->table ) + string->offset ) : NULL );
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
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	struct in_addr netmask_addr = { 0 };
	unsigned int netmask_count = 0;
	int rc;

	/* Extract values from DHCP configuration */
	ibft_set_ipaddr_option ( &nic->ip_address, &ip_setting );
	DBG ( "iBFT NIC IP = %s\n", ibft_ipaddr ( &nic->ip_address ) );
	ibft_set_ipaddr_option ( &nic->gateway, &gateway_setting );
	DBG ( "iBFT NIC gateway = %s\n", ibft_ipaddr ( &nic->gateway ) );
	ibft_set_ipaddr_option ( &nic->dns[0], &dns_setting );
	DBG ( "iBFT NIC DNS = %s\n", ibft_ipaddr ( &nic->dns[0] ) );
	if ( ( rc = ibft_set_string_option ( strings, &nic->hostname,
					     &hostname_setting ) ) != 0 )
		return rc;
	DBG ( "iBFT NIC hostname = %s\n",
	      ibft_string ( strings, &nic->hostname ) );

	/* Derive subnet mask prefix from subnet mask */
	fetch_ipv4_setting ( NULL, &netmask_setting, &netmask_addr );
	while ( netmask_addr.s_addr ) {
		if ( netmask_addr.s_addr & 0x1 )
			netmask_count++;
		netmask_addr.s_addr >>= 1;
	}
	nic->subnet_mask_prefix = netmask_count;
	DBG ( "iBFT NIC subnet = /%d\n", nic->subnet_mask_prefix );

	/* Extract values from net-device configuration */
	if ( ( rc = ll_protocol->eth_addr ( netdev->ll_addr,
					    nic->mac_address ) ) != 0 ) {
		DBG ( "Could not determine iBFT MAC: %s\n", strerror ( rc ) );
		return rc;
	}
	DBG ( "iBFT NIC MAC = %s\n", eth_ntoa ( nic->mac_address ) );
	nic->pci_bus_dev_func = netdev->dev->desc.location;
	DBG ( "iBFT NIC PCI = %04x\n", nic->pci_bus_dev_func );

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
	DBG ( "iBFT initiator hostname = %s\n",
	      ibft_string ( strings, &initiator->initiator_name ) );

	return 0;
}

/**
 * Fill in Target CHAP portion of iBFT
 *
 * @v target		Target portion of iBFT
 * @v strings		iBFT string block descriptor
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int ibft_fill_target_chap ( struct ibft_target *target,
				   struct ibft_string_block *strings,
				   struct iscsi_session *iscsi ) {
	int rc;

	if ( ! ( iscsi->status & ISCSI_STATUS_AUTH_FORWARD_REQUIRED ) )
		return 0;

	assert ( iscsi->initiator_username );
	assert ( iscsi->initiator_password );

	target->chap_type = IBFT_CHAP_ONE_WAY;
	if ( ( rc = ibft_set_string ( strings, &target->chap_name,
				      iscsi->initiator_username ) ) != 0 )
		return rc;
	DBG ( "iBFT target username = %s\n",
	      ibft_string ( strings, &target->chap_name ) );
	if ( ( rc = ibft_set_string ( strings, &target->chap_secret,
				      iscsi->initiator_password ) ) != 0 )
		return rc;
	DBG ( "iBFT target password = <redacted>\n" );

	return 0;
}

/**
 * Fill in Target Reverse CHAP portion of iBFT
 *
 * @v target		Target portion of iBFT
 * @v strings		iBFT string block descriptor
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int ibft_fill_target_reverse_chap ( struct ibft_target *target,
					   struct ibft_string_block *strings,
					   struct iscsi_session *iscsi ) {
	int rc;

	if ( ! ( iscsi->status & ISCSI_STATUS_AUTH_REVERSE_REQUIRED ) )
		return 0;

	assert ( iscsi->initiator_username );
	assert ( iscsi->initiator_password );
	assert ( iscsi->target_username );
	assert ( iscsi->target_password );

	target->chap_type = IBFT_CHAP_MUTUAL;
	if ( ( rc = ibft_set_string ( strings, &target->reverse_chap_name,
				      iscsi->target_username ) ) != 0 )
		return rc;
	DBG ( "iBFT target reverse username = %s\n",
	      ibft_string ( strings, &target->chap_name ) );
	if ( ( rc = ibft_set_string ( strings, &target->reverse_chap_secret,
				      iscsi->target_password ) ) != 0 )
		return rc;
	DBG ( "iBFT target reverse password = <redacted>\n" );

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
	DBG ( "iBFT target IP = %s\n", ibft_ipaddr ( &target->ip_address ) );
	target->socket = ntohs ( sin_target->sin_port );
	DBG ( "iBFT target port = %d\n", target->socket );
	if ( ( rc = ibft_set_string ( strings, &target->target_name,
				      iscsi->target_iqn ) ) != 0 )
		return rc;
	DBG ( "iBFT target name = %s\n",
	      ibft_string ( strings, &target->target_name ) );
	if ( ( rc = ibft_fill_target_chap ( target, strings, iscsi ) ) != 0 )
		return rc;
	if ( ( rc = ibft_fill_target_reverse_chap ( target, strings,
						    iscsi ) ) != 0 )
		return rc;

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
