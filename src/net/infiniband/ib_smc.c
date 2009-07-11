/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <byteswap.h>
#include <gpxe/infiniband.h>
#include <gpxe/ib_smc.h>

/**
 * @file
 *
 * Infiniband Subnet Management Client
 *
 */

/**
 * Get port information
 *
 * @v ibdev		Infiniband device
 * @v local_mad		Method for issuing local MADs
 * @v mad		Management datagram to fill in
 * @ret rc		Return status code
 */
static int ib_smc_get_port_info ( struct ib_device *ibdev,
				  ib_local_mad_t local_mad,
				  union ib_mad *mad ) {
	int rc;

	/* Construct MAD */
	memset ( mad, 0, sizeof ( *mad ) );
	mad->hdr.base_version = IB_MGMT_BASE_VERSION;
	mad->hdr.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED;
	mad->hdr.class_version = 1;
	mad->hdr.method = IB_MGMT_METHOD_GET;
	mad->hdr.attr_id = htons ( IB_SMP_ATTR_PORT_INFO );
	mad->hdr.attr_mod = htonl ( ibdev->port );

	if ( ( rc = local_mad ( ibdev, mad ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not get port info: %s\n",
		       ibdev, strerror ( rc ) );
		return rc;
	}
	return 0;
}

/**
 * Get GUID information
 *
 * @v ibdev		Infiniband device
 * @v local_mad		Method for issuing local MADs
 * @v mad		Management datagram to fill in
 * @ret rc		Return status code
 */
static int ib_smc_get_guid_info ( struct ib_device *ibdev,
				  ib_local_mad_t local_mad,
				  union ib_mad *mad ) {
	int rc;

	/* Construct MAD */
	memset ( mad, 0, sizeof ( *mad ) );
	mad->hdr.base_version = IB_MGMT_BASE_VERSION;
	mad->hdr.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED;
	mad->hdr.class_version = 1;
	mad->hdr.method = IB_MGMT_METHOD_GET;
	mad->hdr.attr_id = htons ( IB_SMP_ATTR_GUID_INFO );

	if ( ( rc = local_mad ( ibdev, mad ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not get GUID info: %s\n",
		       ibdev, strerror ( rc ) );
		return rc;
	}
	return 0;
}

/**
 * Get partition key table
 *
 * @v ibdev		Infiniband device
 * @v local_mad		Method for issuing local MADs
 * @v mad		Management datagram to fill in
 * @ret rc		Return status code
 */
static int ib_smc_get_pkey_table ( struct ib_device *ibdev,
				   ib_local_mad_t local_mad,
				   union ib_mad *mad ) {
	int rc;

	/* Construct MAD */
	memset ( mad, 0, sizeof ( *mad ) );
	mad->hdr.base_version = IB_MGMT_BASE_VERSION;
	mad->hdr.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED;
	mad->hdr.class_version = 1;
	mad->hdr.method = IB_MGMT_METHOD_GET;
	mad->hdr.attr_id = htons ( IB_SMP_ATTR_PKEY_TABLE );

	if ( ( rc = local_mad ( ibdev, mad ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not get pkey table: %s\n",
		       ibdev, strerror ( rc ) );
		return rc;
	}
	return 0;
}

/**
 * Get MAD parameters
 *
 * @v ibdev		Infiniband device
 * @v local_mad		Method for issuing local MADs
 * @ret rc		Return status code
 */
int ib_smc_update ( struct ib_device *ibdev, ib_local_mad_t local_mad ) {
	union ib_mad mad;
	struct ib_port_info *port_info = &mad.smp.smp_data.port_info;
	struct ib_guid_info *guid_info = &mad.smp.smp_data.guid_info;
	struct ib_pkey_table *pkey_table = &mad.smp.smp_data.pkey_table;
	int rc;

	/* Port info gives us the link state, the first half of the
	 * port GID and the SM LID.
	 */
	if ( ( rc = ib_smc_get_port_info ( ibdev, local_mad, &mad ) ) != 0 )
		return rc;
	memcpy ( &ibdev->gid.u.half[0], port_info->gid_prefix,
		 sizeof ( ibdev->gid.u.half[0] ) );
	ibdev->lid = ntohs ( port_info->lid );
	ibdev->sm_lid = ntohs ( port_info->mastersm_lid );
	ibdev->link_width_enabled = port_info->link_width_enabled;
	ibdev->link_width_supported = port_info->link_width_supported;
	ibdev->link_width_active = port_info->link_width_active;
	ibdev->link_speed_supported =
		( port_info->link_speed_supported__port_state >> 4 );
	ibdev->port_state =
		( port_info->link_speed_supported__port_state & 0xf );
	ibdev->link_speed_active =
		( port_info->link_speed_active__link_speed_enabled >> 4 );
	ibdev->link_speed_enabled =
		( port_info->link_speed_active__link_speed_enabled & 0xf );
	ibdev->sm_sl = ( port_info->neighbour_mtu__mastersm_sl & 0xf );

	/* GUID info gives us the second half of the port GID */
	if ( ( rc = ib_smc_get_guid_info ( ibdev, local_mad, &mad ) ) != 0 )
		return rc;
	memcpy ( &ibdev->gid.u.half[1], guid_info->guid[0],
		 sizeof ( ibdev->gid.u.half[1] ) );

	/* Get partition key */
	if ( ( rc = ib_smc_get_pkey_table ( ibdev, local_mad, &mad ) ) != 0 )
		return rc;
	ibdev->pkey = ntohs ( pkey_table->pkey[0] );

	DBGC ( ibdev, "IBDEV %p port GID is %08x:%08x:%08x:%08x\n", ibdev,
	       htonl ( ibdev->gid.u.dwords[0] ),
	       htonl ( ibdev->gid.u.dwords[1] ),
	       htonl ( ibdev->gid.u.dwords[2] ),
	       htonl ( ibdev->gid.u.dwords[3] ) );

	return 0;
}
