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
#include <stdio.h>
#include <unistd.h>
#include <byteswap.h>
#include <gpxe/infiniband.h>
#include <gpxe/ib_gma.h>
#include <gpxe/ib_sma.h>

/**
 * @file
 *
 * Infiniband Subnet Management Agent
 *
 */

/**
 * Get node information
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret rc		Return status code
 */
static int ib_sma_get_node_info ( struct ib_gma *gma,
				  union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_node_info *node_info = &mad->smp.smp_data.node_info;

	memset ( node_info, 0, sizeof ( *node_info ) );
	node_info->base_version = IB_MGMT_BASE_VERSION;
	node_info->class_version = IB_SMP_CLASS_VERSION;
	node_info->node_type = IB_NODE_TYPE_HCA;
	node_info->num_ports = ib_get_hca_info ( ibdev, &node_info->sys_guid );
	memcpy ( &node_info->node_guid, &node_info->sys_guid,
		 sizeof ( node_info->node_guid ) );
	memcpy ( &node_info->port_guid, &ibdev->gid.u.half[1],
		 sizeof ( node_info->port_guid ) );
	node_info->partition_cap = htons ( 1 );
	node_info->local_port_num = ibdev->port;

	return 0;
}

/**
 * Get node description
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret rc		Return status code
 */
static int ib_sma_get_node_desc ( struct ib_gma *gma,
				  union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_node_desc *node_desc = &mad->smp.smp_data.node_desc;
	struct ib_gid_half *guid = &ibdev->gid.u.half[1];

	memset ( node_desc, 0, sizeof ( *node_desc ) );
	snprintf ( node_desc->node_string, sizeof ( node_desc->node_string ),
		   "gPXE %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x (%s)",
		   guid->bytes[0], guid->bytes[1], guid->bytes[2],
		   guid->bytes[3], guid->bytes[4], guid->bytes[5],
		   guid->bytes[6], guid->bytes[7], ibdev->dev->name );

	return 0;
}

/**
 * Get GUID information
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret rc		Return status code
 */
static int ib_sma_get_guid_info ( struct ib_gma *gma,
				  union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_guid_info *guid_info = &mad->smp.smp_data.guid_info;

	memset ( guid_info, 0, sizeof ( *guid_info ) );
	memcpy ( guid_info->guid[0], &ibdev->gid.u.half[1],
		 sizeof ( guid_info->guid[0] ) );

	return 0;
}

/**
 * Get port information
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret rc		Return status code
 */
static int ib_sma_get_port_info ( struct ib_gma *gma,
				  union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_port_info *port_info = &mad->smp.smp_data.port_info;

	memset ( port_info, 0, sizeof ( *port_info ) );
	memcpy ( port_info->gid_prefix, &ibdev->gid.u.half[0],
		 sizeof ( port_info->gid_prefix ) );
	port_info->lid = ntohs ( ibdev->lid );
	port_info->mastersm_lid = ntohs ( ibdev->sm_lid );
	port_info->local_port_num = ibdev->port;
	port_info->link_width_enabled = ibdev->link_width;
	port_info->link_width_supported = ibdev->link_width;
	port_info->link_width_active = ibdev->link_width;
	port_info->link_speed_supported__port_state =
		( ( ibdev->link_speed << 4 ) | ibdev->port_state );
	port_info->port_phys_state__link_down_def_state =
		( ( IB_PORT_PHYS_STATE_POLLING << 4 ) |
		  IB_PORT_PHYS_STATE_POLLING );
	port_info->link_speed_active__link_speed_enabled =
		( ( ibdev->link_speed << 4 ) | ibdev->link_speed );
	port_info->neighbour_mtu__mastersm_sl =
		( ( IB_MTU_2048 << 4 ) | ibdev->sm_sl );
	port_info->vl_cap__init_type = ( IB_VL_0 << 4 );
	port_info->init_type_reply__mtu_cap = IB_MTU_2048;
	port_info->operational_vls__enforcement = ( IB_VL_0 << 4 );
	port_info->guid_cap = 1;

	return 0;
}

/**
 * Set port information
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret rc		Return status code
 */
static int ib_sma_set_port_info ( struct ib_gma *gma,
				  union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	const struct ib_port_info *port_info = &mad->smp.smp_data.port_info;
	int rc;

	memcpy ( &ibdev->gid.u.half[0], port_info->gid_prefix,
		 sizeof ( ibdev->gid.u.half[0] ) );
	ibdev->lid = ntohs ( port_info->lid );
	ibdev->sm_lid = ntohs ( port_info->mastersm_lid );
	ibdev->sm_sl = ( port_info->neighbour_mtu__mastersm_sl & 0xf );

	if ( ( rc = ib_set_port_info ( ibdev, port_info ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not set port information: %s\n",
		       ibdev, strerror ( rc ) );
		mad->hdr.status =
			htons ( IB_MGMT_STATUS_UNSUPPORTED_METHOD_ATTR );
	}

	return ib_sma_get_port_info ( gma, mad );
}

/**
 * Get partition key table
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret rc		Return status code
 */
static int ib_sma_get_pkey_table ( struct ib_gma *gma,
				   union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_pkey_table *pkey_table = &mad->smp.smp_data.pkey_table;

	memset ( pkey_table, 0, sizeof ( *pkey_table ) );
	pkey_table->pkey[0] = htons ( ibdev->pkey );

	return 0;
}

/**
 * Set partition key table
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret rc		Return status code
 */
static int ib_sma_set_pkey_table ( struct ib_gma *gma,
				   union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_pkey_table *pkey_table = &mad->smp.smp_data.pkey_table;

	ibdev->pkey = ntohs ( pkey_table->pkey[0] );

	return ib_sma_get_pkey_table ( gma, mad );
}

/** List of attribute handlers */
struct ib_gma_handler ib_sma_handlers[] __ib_gma_handler = {
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED,
		.mgmt_class_ignore = IB_SMP_CLASS_IGNORE,
		.class_version = IB_SMP_CLASS_VERSION,
		.method = IB_MGMT_METHOD_GET,
		.resp_method = IB_MGMT_METHOD_GET_RESP,
		.attr_id = htons ( IB_SMP_ATTR_NODE_INFO ),
		.handle = ib_sma_get_node_info,
	},
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED,
		.mgmt_class_ignore = IB_SMP_CLASS_IGNORE,
		.class_version = IB_SMP_CLASS_VERSION,
		.method = IB_MGMT_METHOD_GET,
		.resp_method = IB_MGMT_METHOD_GET_RESP,
		.attr_id = htons ( IB_SMP_ATTR_NODE_DESC ),
		.handle = ib_sma_get_node_desc,
	},
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED,
		.mgmt_class_ignore = IB_SMP_CLASS_IGNORE,
		.class_version = IB_SMP_CLASS_VERSION,
		.method = IB_MGMT_METHOD_GET,
		.resp_method = IB_MGMT_METHOD_GET_RESP,
		.attr_id = htons ( IB_SMP_ATTR_GUID_INFO ),
		.handle = ib_sma_get_guid_info,
	},
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED,
		.mgmt_class_ignore = IB_SMP_CLASS_IGNORE,
		.class_version = IB_SMP_CLASS_VERSION,
		.method = IB_MGMT_METHOD_GET,
		.resp_method = IB_MGMT_METHOD_GET_RESP,
		.attr_id = htons ( IB_SMP_ATTR_PORT_INFO ),
		.handle = ib_sma_get_port_info,
	},
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED,
		.mgmt_class_ignore = IB_SMP_CLASS_IGNORE,
		.class_version = IB_SMP_CLASS_VERSION,
		.method = IB_MGMT_METHOD_SET,
		.resp_method = IB_MGMT_METHOD_GET_RESP,
		.attr_id = htons ( IB_SMP_ATTR_PORT_INFO ),
		.handle = ib_sma_set_port_info,
	},
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED,
		.mgmt_class_ignore = IB_SMP_CLASS_IGNORE,
		.class_version = IB_SMP_CLASS_VERSION,
		.method = IB_MGMT_METHOD_GET,
		.resp_method = IB_MGMT_METHOD_GET_RESP,
		.attr_id = htons ( IB_SMP_ATTR_PKEY_TABLE ),
		.handle = ib_sma_get_pkey_table,
	},
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_LID_ROUTED,
		.mgmt_class_ignore = IB_SMP_CLASS_IGNORE,
		.class_version = IB_SMP_CLASS_VERSION,
		.method = IB_MGMT_METHOD_SET,
		.resp_method = IB_MGMT_METHOD_GET_RESP,
		.attr_id = htons ( IB_SMP_ATTR_PKEY_TABLE ),
		.handle = ib_sma_set_pkey_table,
	},
};

/**
 * Create SMA
 *
 * @v sma		Subnet management agent
 * @v ibdev		Infiniband device
 * @v op		Subnet management operations
 * @ret rc		Return status code
 */
int ib_create_sma ( struct ib_sma *sma, struct ib_device *ibdev ) {
	int rc;

	/* Initialise GMA */
	if ( ( rc = ib_create_gma ( &sma->gma, ibdev, IB_QPT_SMA ) ) != 0 ) {
		DBGC ( sma, "SMA %p could not create GMA: %s\n",
		       sma, strerror ( rc ) );
		goto err_create_gma;
	}

	/* If we don't get QP0, we can't function */
	if ( sma->gma.qp->qpn != IB_QPN_SMA ) {
		DBGC ( sma, "SMA %p on QPN %lx, needs to be on QPN 0\n",
		       sma, sma->gma.qp->qpn );
		rc = -ENOTSUP;
		goto err_not_qp0;
	}

	return 0;

 err_not_qp0:
	ib_destroy_gma ( &sma->gma );
 err_create_gma:
	return rc;
}

/**
 * Destroy SMA
 *
 * @v sma		Subnet management agent
 */
void ib_destroy_sma ( struct ib_sma *sma ) {

	ib_destroy_gma ( &sma->gma );
}
