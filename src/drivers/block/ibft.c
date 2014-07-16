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
#include <ipxe/pci.h>
#include <ipxe/acpi.h>
#include <ipxe/in.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/ip.h>
#include <ipxe/ipv6.h>
#include <ipxe/vlan.h>
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
 * An iBFT created by iPXE
 *
 */
struct ipxe_ibft {
	/** The fixed section */
	struct ibft_table table;
	/** The Initiator section */
	struct ibft_initiator initiator __attribute__ (( aligned ( 16 ) ));
	/** The 1. NIC section */
	struct ibft_nic nic0 __attribute__ (( aligned ( 16 ) ));
	/** The 1. Target section */
	struct ibft_target target0 __attribute__ (( aligned ( 16 ) ));
	/** The 2. NIC section */
	struct ibft_nic nic1 __attribute__ (( aligned ( 16 ) ));
	/** The 2. Target section */
	struct ibft_target target1 __attribute__ (( aligned ( 16 ) ));
	/** Strings block */
	char strings[0];
} __attribute__ (( packed, aligned ( 16 ) ));

/**
 * iSCSI string block descriptor
 *
 * This is an internal structure that we use to keep track of the
 * allocation of string data.
 */
struct ibft_strings {
	/** The iBFT containing these strings */
	struct ibft_table *table;
	/** Offset of first free byte within iBFT */
	size_t offset;
	/** Total length of the iBFT */
	size_t len;
};

static int ibft_ipaddr_is_ipv6 ( struct ibft_ipaddr *ipaddr )
{
	uint8_t prefix[12] = { 0, 0, 0, 0, 0, 0,
			       0, 0, 0, 0, 0xff, 0xff };
	return (memcmp(ipaddr->raw, &prefix, sizeof (prefix) ));
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
		ipaddr->in.in = in;
		ipaddr->in.ones = 0xffff;
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
 * Fill in an IPv6 address field within iBFT
 *
 * @v ipaddr		IP address field
 * @v in6		IPv6 address
 */
static void ibft_set_ip6addr ( struct ibft_ipaddr *ipaddr,
			       struct in6_addr in6 ) {
	memset ( ipaddr, 0, sizeof ( *ipaddr ) );
	if ( in6.s6_addr ) {
		ipaddr->in6 = in6;
	}
}

/**
 * Fill in an IPv6 address within iBFT from configuration setting
 *
 * @v settings		Parent settings block, or NULL
 * @v ipaddr		address field
 * @v setting		Configuration setting
 * @v count		Maximum number of IP addresses
 */
static void ibft_set_ip6addr_setting ( struct settings *settings,
				       struct ibft_ipaddr *ipaddr,
				       const struct setting *setting,
				       unsigned int count ) {
	struct in6_addr in6[count];
	unsigned int i;

	fetch_ipv6_array_setting ( settings, setting, in6, count );
	for ( i = 0 ; i < count ; i++ ) {
		ibft_set_ip6addr ( &ipaddr[i], in6[i] );
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
	if ( ibft_ipaddr_is_ipv6(ipaddr) )
		return inet6_ntoa( &ipaddr->in6 );
	else
		return inet_ntoa ( ipaddr->in.in );
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

	if ( ( strings->offset + len ) >= strings->len )
		return NULL;

	string->offset = cpu_to_le16 ( strings->offset );
	string->len = cpu_to_le16 ( len );
	strings->offset += ( len + 1 );

	return ( ( ( char * ) strings->table ) + string->offset );
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
	return ( string->offset ?
		 ( ( ( char * ) strings->table ) + string->offset ) : NULL );
}

/**
 * Fill in IPv4 specific parts of the  NIC portion of iBFT
 *
 * @v nic		NIC portion of iBFT
 * @v dest		IPv4 target address
 * @ret netdev		Return Network device
 */
static struct net_device *ibft_fill_nic_ipv4 ( struct ibft_nic *nic,
					       struct ipv4_miniroute *route4 ) {
	struct settings *parent;
	struct settings *origin;
	struct in_addr netmask_addr = { 0 };
	unsigned int netmask_count = 0;

	/* Determine origin of IP address */
	parent = netdev_settings(route4->netdev);
	fetch_setting ( parent, &ip_setting, &origin, NULL, NULL, 0 );

	nic->origin = ( ( origin == parent ) ?
			IBFT_NIC_ORIGIN_MANUAL : IBFT_NIC_ORIGIN_DHCP );
	DBG ( "iBFT NIC[%d] origin = %d\n",
	      nic->header.index, nic->origin );

	/* Extract values from configuration settings */
	fetch_setting ( parent, &ip_setting, &origin, NULL, NULL, 0 );

	ibft_set_ipaddr ( &nic->ip_address, route4->address );
	DBG ( "iBFT NIC[%d] IP = %s\n", nic->header.index,
	      ibft_ipaddr ( &nic->ip_address ) );
	ibft_set_ipaddr ( &nic->gateway, route4->gateway );
	DBG ( "iBFT NIC[%d] gateway = %s\n", nic->header.index,
	      ibft_ipaddr ( &nic->gateway ) );
	ibft_set_ipaddr_setting ( NULL, &nic->dns[0], &dns_setting,
				  ( sizeof ( nic->dns ) /
				    sizeof ( nic->dns[0] ) ) );
	ibft_set_ipaddr_setting ( parent, &nic->dhcp, &dhcp_server_setting, 1 );
	DBG ( "iBFT NIC[%d] DNS = %s", nic->header.index,
	      ibft_ipaddr ( &nic->dns[0] ) );
	DBG ( ", %s\n", ibft_ipaddr ( &nic->dns[1] ) );
	/* Derive subnet mask prefix from subnet mask */
	netmask_addr = route4->netmask;
	while ( netmask_addr.s_addr ) {
		if ( netmask_addr.s_addr & 0x1 )
			netmask_count++;
		netmask_addr.s_addr >>= 1;
	}
	nic->subnet_mask_prefix = netmask_count;
	DBG ( "iBFT NIC[%d] subnet = /%d\n", nic->header.index,
	      nic->subnet_mask_prefix );
	return route4->netdev;
}

/**
 * Fill in IPv6 specific parts of the NIC portion of iBFT
 *
 * @v nic		NIC portion of iBFT
 * @v dest		IPv6 destination address
 * @ret rc		Return network device
 */
static struct net_device * ibft_fill_nic_ipv6 ( struct ibft_nic *nic,
						struct ipv6_miniroute *route6,
						struct in6_addr * router ) {
	struct settings *parent;
	struct settings *origin;

	parent = netdev_settings(route6->netdev);
	fetch_setting ( parent, &ip6_setting, &origin, NULL, NULL, 0 );
	nic->origin = ( ( origin == parent ) ?
			IBFT_NIC_ORIGIN_MANUAL : IBFT_NIC_ORIGIN_DHCP );
	DBG ( "iBFT NIC[%d] origin = %d\n",
	      nic->header.index, nic->origin );

	/* Extract values from configuration settings */
	ibft_set_ip6addr ( &nic->ip_address, route6->address );
	DBG ( "iBFT NIC[%d] IP = %s\n",
	      nic->header.index, ibft_ipaddr ( &nic->ip_address ) );
	ibft_set_ip6addr ( &nic->gateway, *router );
	DBG ( "iBFT NIC[%d] gateway = %s\n",
	      nic->header.index, ibft_ipaddr ( &nic->gateway ) );
	ibft_set_ip6addr_setting ( NULL, &nic->dns[0], &dns6_setting,
				   ( sizeof ( nic->dns ) /
				     sizeof ( nic->dns[0] ) ) );
	DBG ( "iBFT NIC[%d] DNS = %s", nic->header.index,
	      ibft_ipaddr ( &nic->dns[0] ) );
	DBG ( ", %s\n", ibft_ipaddr ( &nic->dns[1] ) );
	nic->subnet_mask_prefix = route6->prefix_len;
	DBG ( "iBFT NIC[%d] subnet = /%d\n",
	      nic->header.index, nic->subnet_mask_prefix );

	return route6->netdev;
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
			   struct iscsi_session *iscsi,
			   int nic_index ) {
	struct sockaddr_in *sin_target =
		( struct sockaddr_in * ) &iscsi->target_sockaddr;
	struct sockaddr_in6 *sin6_target =
		( struct sockaddr_in6 * ) &iscsi->target_sockaddr;
	struct ipv4_miniroute * route4 = NULL;
	struct ipv6_miniroute * route6 = NULL;
	struct in6_addr *router6;
	struct net_device *netdev;
	struct ll_protocol *ll_protocol;
	int rc;

	if (sin_target->sin_family == AF_INET) {
		struct in_addr router4 = sin_target->sin_addr;
		route4 = ipv4_route(sin_target->sin_scope_id, &router4);
		if (!route4) {
			DBG ( "iBFT NIC[%d] IPv6 host unreachable\n",
				nic_index );
			return -EHOSTUNREACH;
		}
		if (nic->header.flags & IBFT_FL_NIC_BLOCK_VALID) {
			struct ibft_ipaddr tmp;
			ibft_set_ipaddr( &tmp, route4->address );
			if (memcmp(&nic->ip_address, &tmp, sizeof(tmp)))
				return -EBUSY;
		}
	} else if (sin6_target->sin6_family == AF_INET6) {
		router6 = &sin6_target->sin6_addr;
		route6 = ipv6_route(sin6_target->sin6_scope_id, &router6);
		if (!route6) {
			DBG ( "iBFT NIC[%d] IPv6 host unreachable\n",
				nic_index );
			return -EHOSTUNREACH;
		}
		if (nic->header.flags & IBFT_FL_NIC_BLOCK_VALID) {
			struct ibft_ipaddr tmp;
			ibft_set_ip6addr( &tmp, route6->address );
			if (memcmp(&nic->ip_address, &tmp, sizeof(tmp)))
				return -EBUSY;
		}
	} else
		return -EPROTONOSUPPORT;


	/* Fill in common header */
	nic->header.structure_id = IBFT_STRUCTURE_ID_NIC;
	nic->header.version = 1;
	nic->header.index = nic_index;
	nic->header.length = cpu_to_le16 ( sizeof ( *nic ) );
	nic->header.flags = ( IBFT_FL_NIC_BLOCK_VALID |
			      IBFT_FL_NIC_FIRMWARE_BOOT_SELECTED );

	if (sin_target->sin_family == AF_INET)
		netdev = ibft_fill_nic_ipv4( nic, route4 );
	else
		netdev = ibft_fill_nic_ipv6( nic, route6, router6 );

	/* Extract values from net-device configuration */
	nic->vlan = cpu_to_le16 ( vlan_tag ( netdev ) );
	DBG ( "iBFT NIC[%d] VLAN = %02x\n",
	      nic->header.index, le16_to_cpu ( nic->vlan ) );
	ll_protocol = netdev->ll_protocol;
	if ( ( rc = ll_protocol->eth_addr ( netdev->ll_addr,
					    nic->mac_address ) ) != 0 ) {
		DBG ( "Could not determine iBFT MAC: %s\n", strerror ( rc ) );
		return rc;
	}
	DBG ( "iBFT NIC[%d] MAC = %s\n",
	      nic->header.index, eth_ntoa ( nic->mac_address ) );
	nic->pci_bus_dev_func = cpu_to_le16 ( netdev->dev->desc.location );
	DBG ( "iBFT NIC[%d] PCI = %04x\n",
	      nic->header.index, le16_to_cpu ( nic->pci_bus_dev_func ) );

	if ( ( rc = ibft_set_string_setting ( NULL, strings, &nic->hostname,
					      &hostname_setting ) ) != 0 )
		return rc;
	DBG ( "iBFT NIC[%d] hostname = %s\n",
	      nic->header.index,
	      ibft_string ( strings, &nic->hostname ) );

	return 0;
}

/**
 * Fill in Initiator portion of iBFT
 *
 * @v initiator		Initiator portion of iBFT
 * @v strings		iBFT string block descriptor
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int ibft_fill_initiator ( struct ibft_initiator *initiator,
				 struct ibft_strings *strings,
				 struct iscsi_session *iscsi ) {
	int rc;

	/* Fill in common header */
	initiator->header.structure_id = IBFT_STRUCTURE_ID_INITIATOR;
	initiator->header.version = 1;
	initiator->header.length = cpu_to_le16 ( sizeof ( *initiator ) );
	initiator->header.flags = ( IBFT_FL_INITIATOR_BLOCK_VALID |
				    IBFT_FL_INITIATOR_FIRMWARE_BOOT_SELECTED );

	/* Fill in hostname */
	if ( ( rc = ibft_set_string ( strings, &initiator->initiator_name,
				      iscsi->initiator_iqn ) ) != 0 )
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
	DBG ( "iBFT target[%d] username = %s\n",
	      target->header.index,
	      ibft_string ( strings, &target->chap_name ) );
	if ( ( rc = ibft_set_string ( strings, &target->chap_secret,
				      iscsi->initiator_password ) ) != 0 )
		return rc;
	DBG ( "iBFT target[%d] password = <redacted>\n",
		target->header.index );

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
	DBG ( "iBFT target[%d] reverse username = %s\n",
	      target->header.index,
	      ibft_string ( strings, &target->chap_name ) );
	if ( ( rc = ibft_set_string ( strings, &target->reverse_chap_secret,
				      iscsi->target_password ) ) != 0 )
		return rc;
	DBG ( "iBFT target[%d] reverse password = <redacted>\n",
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
			      struct iscsi_session *iscsi,
			      int ibft_index ) {
	struct sockaddr_in *sin_target =
		( struct sockaddr_in * ) &iscsi->target_sockaddr;
	struct sockaddr_in6 *sin6_target =
		( struct sockaddr_in6 * ) &iscsi->target_sockaddr;
	int rc;

	if (sin_target->sin_family == AF_INET) {
		if (target->header.flags & IBFT_FL_TARGET_BLOCK_VALID) {
			struct ibft_ipaddr tmp;
			ibft_set_ipaddr ( &tmp, sin_target->sin_addr );
			if ( memcmp( &target->ip_address, &tmp, sizeof(tmp) ))
				return -EBUSY;
		}
	} else if (sin6_target->sin6_family == AF_INET6) {
		if (target->header.flags & IBFT_FL_TARGET_BLOCK_VALID) {
			struct ibft_ipaddr tmp;
			ibft_set_ip6addr ( &tmp, sin6_target->sin6_addr );
			if ( memcmp( &target->ip_address, &tmp, sizeof(tmp) ))
				return -EBUSY;
		}
	} else {
		DBG ( "iBFT invalid IP address type %d\n",
		      sin6_target->sin6_family );
		return -EPROTONOSUPPORT;
	}
	/* Fill in common header */
	target->header.structure_id = IBFT_STRUCTURE_ID_TARGET;
	target->header.version = 1;
	target->header.index = ibft_index;
	target->header.length = cpu_to_le16 ( sizeof ( *target ) );
	target->header.flags = ( IBFT_FL_TARGET_BLOCK_VALID |
				 IBFT_FL_TARGET_FIRMWARE_BOOT_SELECTED );

	target->nic_association = ibft_index;
	/* Fill in Target values */
	if (sin_target->sin_family == AF_INET) {
		ibft_set_ipaddr ( &target->ip_address, sin_target->sin_addr );
		DBG ( "iBFT target[%d] IP = %s\n", target->header.index,
		      ibft_ipaddr ( &target->ip_address ) );
		target->socket = cpu_to_le16 ( ntohs ( sin_target->sin_port ) );
		DBG ( "iBFT target[%d] port = %d\n", target->header.index,
		      target->socket );
	} else {
		ibft_set_ip6addr ( &target->ip_address, sin6_target->sin6_addr );
		DBG ( "iBFT target[%d] IP = %s\n", target->header.index,
		      ibft_ipaddr ( &target->ip_address ) );
		target->socket = cpu_to_le16 ( ntohs ( sin6_target->sin6_port ) );
		DBG ( "iBFT target[%d] port = %d\n", target->header.index,
		      target->socket );
	}
	memcpy ( &target->boot_lun, &iscsi->lun, sizeof ( target->boot_lun ) );
	DBG ( "iBFT target[%d] boot LUN = " SCSI_LUN_FORMAT "\n",
	      target->header.index, SCSI_LUN_DATA ( target->boot_lun ) );
	if ( ( rc = ibft_set_string ( strings, &target->target_name,
				      iscsi->target_iqn ) ) != 0 )
		return rc;
	DBG ( "iBFT target[%d] name = %s\n", target->header.index,
	      ibft_string ( strings, &target->target_name ) );
	if ( ( rc = ibft_fill_target_chap ( target, strings, iscsi ) ) != 0 )
		return rc;
	if ( ( rc = ibft_fill_target_reverse_chap ( target, strings,
						    iscsi ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Fill in iBFT
 *
 * @v iscsi		iSCSI session
 * @v acpi		ACPI table
 * @v len		Length of ACPI table
 * @ret rc		Return status code
 */
int ibft_describe ( struct iscsi_session *iscsi,
		    struct acpi_description_header *acpi,
		    size_t len ) {
	struct ipxe_ibft *ibft =
		container_of ( acpi, struct ipxe_ibft, table.acpi );
	struct ibft_strings strings = {
		.table = &ibft->table,
		.offset = offsetof ( typeof ( *ibft ), strings ),
		.len = len,
	};
	struct iscsi_session *tmp_iscsi;
	int ibft_index = 0;
	int rc;

	/* Fill in ACPI header */
	DBG ( "iBFT table %p\n", ibft);
	ibft->table.acpi.signature = cpu_to_le32 ( IBFT_SIG );
	ibft->table.acpi.length = cpu_to_le32 ( len );
	ibft->table.acpi.revision = 1;

	/* Fill in Control block */
	ibft->table.control.header.structure_id = IBFT_STRUCTURE_ID_CONTROL;
	ibft->table.control.header.version = 1;
	ibft->table.control.header.length =
		cpu_to_le16 ( sizeof ( ibft->table.control ) );
	ibft->table.control.initiator =
		cpu_to_le16 ( offsetof ( typeof ( *ibft ), initiator ) );
	ibft->table.control.nic_0 =
		cpu_to_le16 ( offsetof ( typeof ( *ibft ), nic0 ) );
	ibft->table.control.target_0 =
		cpu_to_le16 ( offsetof ( typeof ( *ibft ), target0 ) );
	ibft->table.control.nic_1 =
		cpu_to_le16 ( offsetof ( typeof ( *ibft ), nic1 ) );
	ibft->table.control.target_1 =
		cpu_to_le16 ( offsetof ( typeof ( *ibft ), target1 ) );

	/* Fill in Initiator block */
	if ( ( rc = ibft_fill_initiator ( &ibft->initiator, &strings,
					  iscsi ) ) != 0 )
		return rc;

	list_for_each_entry( tmp_iscsi, &iscsi_session_list, node ) {
		struct ibft_nic *nic;
		struct ibft_target *target;

		if ( ibft_index == 0 ) {
			nic = &ibft->nic0;
			target = &ibft->target0;
		} else {
			nic = &ibft->nic1;
			target = &ibft->target1;
		}
		/* Fill in NIC blocks */
		rc = ibft_fill_nic ( nic, &strings,
				     tmp_iscsi, ibft_index );
		if (rc == 0)
			/* Fill in target blocks */
			rc = ibft_fill_target ( target, &strings, tmp_iscsi,
						ibft_index );
		if ( rc != 0 )
			break;
		ibft_index++;
	}

	return rc;
}
