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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/pci.h>
#include <ipxe/acpi.h>
#include <ipxe/in.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/vlan.h>
#include <ipxe/tcpip.h>
#include <ipxe/dhcp.h>
#include <ipxe/iscsi.h>
#include <ipxe/ibft.h>

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

/**
 * iSCSI string buffer
 *
 * This is an internal structure that we use to keep track of the
 * allocation of string data.
 */
struct ibft_strings {
	/** Strings data */
	char *data;
	/** Starting offset of strings */
	size_t start;
	/** Total length */
	size_t len;
};

/**
 * Align structure within iBFT
 *
 * @v len		Unaligned length (or offset)
 * @ret len		Aligned length (or offset)
 */
static inline size_t ibft_align ( size_t len ) {

	return ( ( len + IBFT_ALIGN - 1 ) & ~( IBFT_ALIGN - 1 ) );
}

/**
 * Fill in an IP address field within iBFT
 *
 * @v ipaddr		IP address field
 * @v in		IPv4 address
 */
static void ibft_set_ipaddr ( struct ibft_ipaddr *ipaddr, struct in_addr in ) {
	memset ( ipaddr, 0, sizeof ( *ipaddr ) );
	if ( in.s_addr ) {
		ipaddr->in = in;
		ipaddr->ones = 0xffff;
	}
}

/**
 * Fill in an IP address within iBFT from configuration setting
 *
 * @v settings		Parent settings block, or NULL
 * @v ipaddr		IP address field
 * @v setting		Configuration setting
 * @v count		Maximum number of IP addresses
 */
static void ibft_set_ipaddr_setting ( struct settings *settings,
				      struct ibft_ipaddr *ipaddr,
				      const struct setting *setting,
				      unsigned int count ) {
	struct in_addr in[count];
	unsigned int i;

	fetch_ipv4_array_setting ( settings, setting, in, count );
	for ( i = 0 ; i < count ; i++ ) {
		ibft_set_ipaddr ( &ipaddr[i], in[i] );
	}
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
 * @ret dest		String destination, or NULL
 */
static char * ibft_alloc_string ( struct ibft_strings *strings,
				  struct ibft_string *string, size_t len ) {
	size_t new_len;
	char *new_data;
	char *dest;

	/* Extend string data buffer */
	new_len = ( strings->len + len + 1 /* NUL */ );
	new_data = realloc ( strings->data, new_len );
	if ( ! new_data )
		return NULL;
	strings->data = new_data;

	/* Fill in string field */
	string->offset = cpu_to_le16 ( strings->start + strings->len );
	string->len = cpu_to_le16 ( len );

	/* Zero string */
	dest = ( strings->data + strings->len );
	memset ( dest, 0, ( len + 1 /* NUL */ ) );

	/* Update allocated length */
	strings->len = new_len;

	return dest;
}

/**
 * Fill in a string field within iBFT
 *
 * @v strings		iBFT string block descriptor
 * @v string		String field
 * @v data		String to fill in, or NULL
 * @ret rc		Return status code
 */
static int ibft_set_string ( struct ibft_strings *strings,
			     struct ibft_string *string, const char *data ) {
	char *dest;

	if ( ! data )
		return 0;

	dest = ibft_alloc_string ( strings, string, strlen ( data ) );
	if ( ! dest )
		return -ENOBUFS;
	strcpy ( dest, data );

	return 0;
}

/**
 * Fill in a string field within iBFT from configuration setting
 *
 * @v settings		Parent settings block, or NULL
 * @v strings		iBFT string block descriptor
 * @v string		String field
 * @v setting		Configuration setting
 * @ret rc		Return status code
 */
static int ibft_set_string_setting ( struct settings *settings,
				     struct ibft_strings *strings,
				     struct ibft_string *string,
				     const struct setting *setting ) {
	struct settings *origin;
	struct setting fetched;
	int len;
	char *dest;

	len = fetch_setting ( settings, setting, &origin, &fetched, NULL, 0 );
	if ( len < 0 ) {
		string->offset = 0;
		string->len = 0;
		return 0;
	}

	dest = ibft_alloc_string ( strings, string, len );
	if ( ! dest )
		return -ENOBUFS;
	fetch_string_setting ( origin, &fetched, dest, ( len + 1 ));

	return 0;
}

/**
 * Read string from iBFT (for debugging)
 *
 * @v strings		iBFT string block descriptor
 * @v string		String field
 * @ret data		String content (or "<empty>")
 */
static const char * ibft_string ( struct ibft_strings *strings,
				  struct ibft_string *string ) {
	size_t offset = le16_to_cpu ( string->offset );

	return ( offset ? ( strings->data + offset - strings->start ) : NULL );
}

/**
 * Check if network device is required for the iBFT
 *
 * @v netdev		Network device
 * @ret is_required	Network device is required
 */
static int ibft_netdev_is_required ( struct net_device *netdev ) {
	struct iscsi_session *iscsi;
	struct sockaddr_tcpip *st_target;

	list_for_each_entry ( iscsi, &ibft_model.descs, desc.list ) {
		st_target = ( struct sockaddr_tcpip * ) &iscsi->target_sockaddr;
		if ( tcpip_netdev ( st_target ) == netdev )
			return 1;
	}

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
			   struct ibft_strings *strings,
			   struct net_device *netdev ) {
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	struct in_addr netmask_addr = { 0 };
	unsigned int netmask_count = 0;
	struct settings *parent = netdev_settings ( netdev );
	struct settings *origin;
	int rc;

	/* Fill in common header */
	nic->header.structure_id = IBFT_STRUCTURE_ID_NIC;
	nic->header.version = 1;
	nic->header.length = cpu_to_le16 ( sizeof ( *nic ) );
	nic->header.flags = ( IBFT_FL_NIC_BLOCK_VALID |
			      IBFT_FL_NIC_FIRMWARE_BOOT_SELECTED );
	DBG ( "iBFT NIC %d is %s\n", nic->header.index, netdev->name );

	/* Determine origin of IP address */
	fetch_setting ( parent, &ip_setting, &origin, NULL, NULL, 0 );
	nic->origin = ( ( origin == parent ) ?
			IBFT_NIC_ORIGIN_MANUAL : IBFT_NIC_ORIGIN_DHCP );
	DBG ( "iBFT NIC %d origin = %d\n", nic->header.index, nic->origin );

	/* Extract values from configuration settings */
	ibft_set_ipaddr_setting ( parent, &nic->ip_address, &ip_setting, 1 );
	DBG ( "iBFT NIC %d IP = %s\n",
	      nic->header.index, ibft_ipaddr ( &nic->ip_address ) );
	ibft_set_ipaddr_setting ( parent, &nic->gateway, &gateway_setting, 1 );
	DBG ( "iBFT NIC %d gateway = %s\n",
	      nic->header.index, ibft_ipaddr ( &nic->gateway ) );
	ibft_set_ipaddr_setting ( NULL, &nic->dns[0], &dns_setting,
				  ( sizeof ( nic->dns ) /
				    sizeof ( nic->dns[0] ) ) );
	ibft_set_ipaddr_setting ( parent, &nic->dhcp, &dhcp_server_setting, 1 );
	DBG ( "iBFT NIC %d DNS = %s",
	      nic->header.index, ibft_ipaddr ( &nic->dns[0] ) );
	DBG ( ", %s\n", ibft_ipaddr ( &nic->dns[1] ) );
	if ( ( rc = ibft_set_string_setting ( NULL, strings, &nic->hostname,
					      &hostname_setting ) ) != 0 )
		return rc;
	DBG ( "iBFT NIC %d hostname = %s\n",
	      nic->header.index, ibft_string ( strings, &nic->hostname ) );

	/* Derive subnet mask prefix from subnet mask */
	fetch_ipv4_setting ( parent, &netmask_setting, &netmask_addr );
	while ( netmask_addr.s_addr ) {
		if ( netmask_addr.s_addr & 0x1 )
			netmask_count++;
		netmask_addr.s_addr >>= 1;
	}
	nic->subnet_mask_prefix = netmask_count;
	DBG ( "iBFT NIC %d subnet = /%d\n",
	      nic->header.index, nic->subnet_mask_prefix );

	/* Extract values from net-device configuration */
	nic->vlan = cpu_to_le16 ( vlan_tag ( netdev ) );
	DBG ( "iBFT NIC %d VLAN = %02x\n",
	      nic->header.index, le16_to_cpu ( nic->vlan ) );
	if ( ( rc = ll_protocol->eth_addr ( netdev->ll_addr,
					    nic->mac_address ) ) != 0 ) {
		DBG ( "Could not determine %s MAC: %s\n",
		      netdev->name, strerror ( rc ) );
		return rc;
	}
	DBG ( "iBFT NIC %d MAC = %s\n",
	      nic->header.index, eth_ntoa ( nic->mac_address ) );
	nic->pci_bus_dev_func = cpu_to_le16 ( netdev->dev->desc.location );
	DBG ( "iBFT NIC %d PCI = %04x\n",
	      nic->header.index, le16_to_cpu ( nic->pci_bus_dev_func ) );

	return 0;
}

/**
 * Fill in Initiator portion of iBFT
 *
 * @v initiator		Initiator portion of iBFT
 * @v strings		iBFT string block descriptor
 * @v initiator_iqn	Initiator IQN
 * @ret rc		Return status code
 */
static int ibft_fill_initiator ( struct ibft_initiator *initiator,
				 struct ibft_strings *strings,
				 const char *initiator_iqn ) {
	int rc;

	/* Fill in common header */
	initiator->header.structure_id = IBFT_STRUCTURE_ID_INITIATOR;
	initiator->header.version = 1;
	initiator->header.length = cpu_to_le16 ( sizeof ( *initiator ) );
	initiator->header.flags = ( IBFT_FL_INITIATOR_BLOCK_VALID |
				    IBFT_FL_INITIATOR_FIRMWARE_BOOT_SELECTED );

	/* Fill in initiator name */
	if ( ( rc = ibft_set_string ( strings, &initiator->initiator_name,
				      initiator_iqn ) ) != 0 )
		return rc;
	DBG ( "iBFT initiator name = %s\n",
	      ibft_string ( strings, &initiator->initiator_name ) );

	return 0;
}

/**
 * Fill in Target NIC association
 *
 * @v target		Target portion of iBFT
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int ibft_fill_target_nic_association ( struct ibft_target *target,
					      struct iscsi_session *iscsi ) {
	struct sockaddr_tcpip *st_target =
		( struct sockaddr_tcpip * ) &iscsi->target_sockaddr;
	struct net_device *associated;
	struct net_device *netdev;

	/* Find network device used to reach target */
	associated = tcpip_netdev ( st_target );
	if ( ! associated ) {
		DBG ( "iBFT target %d has no net device\n",
		      target->header.index );
		return -EHOSTUNREACH;
	}

	/* Calculate association */
	for_each_netdev ( netdev ) {
		if ( netdev == associated ) {
			DBG ( "iBFT target %d uses NIC %d (%s)\n",
			      target->header.index, target->nic_association,
			      netdev->name );
			return 0;
		}
		if ( ! ibft_netdev_is_required ( netdev ) )
			continue;
		target->nic_association++;
	}

	DBG ( "iBFT target %d has impossible NIC %s\n",
	      target->header.index, netdev->name );
	return -EINVAL;
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
				   struct ibft_strings *strings,
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
	DBG ( "iBFT target %d username = %s\n", target->header.index,
	      ibft_string ( strings, &target->chap_name ) );
	if ( ( rc = ibft_set_string ( strings, &target->chap_secret,
				      iscsi->initiator_password ) ) != 0 )
		return rc;
	DBG ( "iBFT target %d password = <redacted>\n", target->header.index );

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
					   struct ibft_strings *strings,
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
	DBG ( "iBFT target %d reverse username = %s\n", target->header.index,
	      ibft_string ( strings, &target->chap_name ) );
	if ( ( rc = ibft_set_string ( strings, &target->reverse_chap_secret,
				      iscsi->target_password ) ) != 0 )
		return rc;
	DBG ( "iBFT target %d reverse password = <redacted>\n",
	      target->header.index );

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
			      struct ibft_strings *strings,
			      struct iscsi_session *iscsi ) {
	struct sockaddr_tcpip *st_target =
		( struct sockaddr_tcpip * ) &iscsi->target_sockaddr;
	struct sockaddr_in *sin_target =
		( struct sockaddr_in * ) &iscsi->target_sockaddr;
	int rc;

	/* Fill in common header */
	target->header.structure_id = IBFT_STRUCTURE_ID_TARGET;
	target->header.version = 1;
	target->header.length = cpu_to_le16 ( sizeof ( *target ) );
	target->header.flags = ( IBFT_FL_TARGET_BLOCK_VALID |
				 IBFT_FL_TARGET_FIRMWARE_BOOT_SELECTED );

	/* Fill in Target values */
	ibft_set_ipaddr ( &target->ip_address, sin_target->sin_addr );
	DBG ( "iBFT target %d IP = %s\n",
	      target->header.index, ibft_ipaddr ( &target->ip_address ) );
	target->socket = cpu_to_le16 ( ntohs ( st_target->st_port ) );
	DBG ( "iBFT target %d port = %d\n",
	      target->header.index, target->socket );
	memcpy ( &target->boot_lun, &iscsi->lun, sizeof ( target->boot_lun ) );
	DBG ( "iBFT target %d boot LUN = " SCSI_LUN_FORMAT "\n",
	      target->header.index, SCSI_LUN_DATA ( target->boot_lun ) );
	if ( ( rc = ibft_set_string ( strings, &target->target_name,
				      iscsi->target_iqn ) ) != 0 )
		return rc;
	DBG ( "iBFT target %d name = %s\n", target->header.index,
	      ibft_string ( strings, &target->target_name ) );
	if ( ( rc = ibft_fill_target_nic_association ( target, iscsi ) ) != 0 )
		return rc;
	if ( ( rc = ibft_fill_target_chap ( target, strings, iscsi ) ) != 0 )
		return rc;
	if ( ( rc = ibft_fill_target_reverse_chap ( target, strings,
						    iscsi ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Check if iBFT descriptor is complete
 *
 * @v desc		ACPI descriptor
 * @ret rc		Return status code
 */
static int ibft_complete ( struct acpi_descriptor *desc ) {
	struct iscsi_session *iscsi =
		container_of ( desc, struct iscsi_session, desc );

	/* Fail if we do not yet have the target address */
	if ( ! iscsi->target_sockaddr.sa_family )
		return -EAGAIN;

	return 0;
}

/**
 * Install iBFT
 *
 * @v install		Installation method
 * @ret rc		Return status code
 */
static int ibft_install ( int ( * install ) ( struct acpi_header *acpi ) ) {
	struct net_device *netdev;
	struct iscsi_session *iscsi;
	struct ibft_table *table;
	struct ibft_initiator *initiator;
	struct ibft_nic *nic;
	struct ibft_target *target;
	struct ibft_strings strings;
	struct acpi_header *acpi;
	void *data;
	unsigned int targets = 0;
	unsigned int pairs = 0;
	size_t offset = 0;
	size_t table_len;
	size_t control_len;
	size_t initiator_offset;
	size_t nic_offset;
	size_t target_offset;
	size_t strings_offset;
	size_t len;
	unsigned int i;
	int rc;

	/* Calculate table sizes and offsets */
	list_for_each_entry ( iscsi, &ibft_model.descs, desc.list )
		targets++;
	pairs = ( sizeof ( table->control.pair ) /
		  sizeof ( table->control.pair[0] ) );
	if ( pairs < targets )
		pairs = targets;
	offset = offsetof ( typeof ( *table ), control.pair );
	offset += ( pairs * sizeof ( table->control.pair[0] ) );
	table_len = offset;
	control_len = ( table_len - offsetof ( typeof ( *table ), control ) );
	offset = ibft_align ( offset );
	initiator_offset = offset;
	offset += ibft_align ( sizeof ( *initiator ) );
	nic_offset = offset;
	offset += ( pairs * ibft_align ( sizeof ( *nic ) ) );
	target_offset = offset;
	offset += ( pairs * ibft_align ( sizeof ( *target ) ) );
	strings_offset = offset;
	strings.data = NULL;
	strings.start = strings_offset;
	strings.len = 0;
	len = offset;

	/* Do nothing if no targets exist */
	if ( ! targets ) {
		rc = 0;
		goto no_targets;
	}

	/* Allocate table */
	data = zalloc ( len );
	if ( ! data ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Fill in Control block */
	table = data;
	table->control.header.structure_id = IBFT_STRUCTURE_ID_CONTROL;
	table->control.header.version = 1;
	table->control.header.length = cpu_to_le16 ( control_len );

	/* Fill in Initiator block */
	initiator = ( data + initiator_offset );
	table->control.initiator = cpu_to_le16 ( initiator_offset );
	iscsi = list_first_entry ( &ibft_model.descs, struct iscsi_session,
				   desc.list );
	if ( ( rc = ibft_fill_initiator ( initiator, &strings,
					  iscsi->initiator_iqn ) ) != 0 )
		goto err_initiator;

	/* Fill in NIC blocks */
	i = 0;
	for_each_netdev ( netdev ) {
		if ( ! ibft_netdev_is_required ( netdev ) )
			continue;
		assert ( i < pairs );
		table->control.pair[i].nic = nic_offset;
		nic = ( data + nic_offset );
		nic->header.index = i;
		if ( ( rc = ibft_fill_nic ( nic, &strings, netdev ) ) != 0 )
			goto err_nic;
		i++;
		nic_offset += ibft_align ( sizeof ( *nic ) );
	}

	/* Fill in Target blocks */
	i = 0;
	list_for_each_entry ( iscsi, &ibft_model.descs, desc.list ) {
		assert ( i < pairs );
		table->control.pair[i].target = target_offset;
		target = ( data + target_offset );
		target->header.index = i;
		if ( ( rc = ibft_fill_target ( target, &strings, iscsi ) ) != 0)
			goto err_target;
		i++;
		target_offset += ibft_align ( sizeof ( *target ) );
	}

	/* Reallocate table to include space for strings */
	len += strings.len;
	acpi = realloc ( data, len );
	if ( ! acpi )
		goto err_realloc;
	data = NULL;

	/* Fill in ACPI header */
	acpi->signature = cpu_to_le32 ( IBFT_SIG );
	acpi->length = cpu_to_le32 ( len );
	acpi->revision = 1;

	/* Append strings */
	memcpy ( ( ( ( void * ) acpi ) + strings_offset ), strings.data,
		 strings.len );

	/* Install ACPI table */
	if ( ( rc = install ( acpi ) ) != 0 ) {
		DBG ( "iBFT could not install: %s\n", strerror ( rc ) );
		goto err_install;
	}

 err_install:
	free ( acpi );
 err_realloc:
 err_target:
 err_nic:
 err_initiator:
	free ( data );
 err_alloc:
 no_targets:
	free ( strings.data );
	return rc;
}

/** iBFT model */
struct acpi_model ibft_model __acpi_model = {
	.descs = LIST_HEAD_INIT ( ibft_model.descs ),
	.complete = ibft_complete,
	.install = ibft_install,
};
