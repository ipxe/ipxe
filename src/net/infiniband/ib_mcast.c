/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <string.h>
#include <byteswap.h>
#include <errno.h>
#include <gpxe/list.h>
#include <gpxe/infiniband.h>
#include <gpxe/ib_gma.h>
#include <gpxe/ib_mcast.h>

/** @file
 *
 * Infiniband multicast groups
 *
 */

/**
 * Transmit multicast group membership request
 *
 * @v gma		General management agent
 * @v gid		Multicast GID
 * @v join		Join (rather than leave) group
 * @ret rc		Return status code
 */
static int ib_mc_member_request ( struct ib_gma *gma, struct ib_gid *gid,
				  int join ) {
	union ib_mad mad;
	struct ib_mad_sa *sa = &mad.sa;
	int rc;

	/* Construct multicast membership record request */
	memset ( sa, 0, sizeof ( *sa ) );
	sa->mad_hdr.base_version = IB_MGMT_BASE_VERSION;
	sa->mad_hdr.mgmt_class = IB_MGMT_CLASS_SUBN_ADM;
	sa->mad_hdr.class_version = IB_SA_CLASS_VERSION;
	sa->mad_hdr.method =
		( join ? IB_MGMT_METHOD_SET : IB_MGMT_METHOD_DELETE );
	sa->mad_hdr.attr_id = htons ( IB_SA_ATTR_MC_MEMBER_REC );
	sa->sa_hdr.comp_mask[1] =
		htonl ( IB_SA_MCMEMBER_REC_MGID | IB_SA_MCMEMBER_REC_PORT_GID |
			IB_SA_MCMEMBER_REC_JOIN_STATE );
	sa->sa_data.mc_member_record.scope__join_state = 1;
	memcpy ( &sa->sa_data.mc_member_record.mgid, gid,
		 sizeof ( sa->sa_data.mc_member_record.mgid ) );
	memcpy ( &sa->sa_data.mc_member_record.port_gid, &gma->ibdev->gid,
		 sizeof ( sa->sa_data.mc_member_record.port_gid ) );

	/* Issue multicast membership record request */
	if ( ( rc = ib_gma_request ( gma, &mad, NULL, join ) ) != 0 ) {
		DBGC ( gma, "GMA %p could not join group: %s\n",
		       gma, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Join multicast group
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v gid		Multicast GID
 * @ret rc		Return status code
 */
int ib_mcast_join ( struct ib_device *ibdev, struct ib_queue_pair *qp,
		    struct ib_gid *gid ) {
	struct ib_gma *gma = ibdev->gma;
	int rc;

	DBGC ( gma, "GMA %p QPN %lx joining %08x:%08x:%08x:%08x\n",
	       gma, qp->qpn, ntohl ( gid->u.dwords[0] ),
	       ntohl ( gid->u.dwords[1] ), ntohl ( gid->u.dwords[2] ),
	       ntohl ( gid->u.dwords[3] ) );

	/* Attach queue pair to multicast GID */
	if ( ( rc = ib_mcast_attach ( ibdev, qp, gid ) ) != 0 ) {
		DBGC ( gma, "GMA %p could not attach: %s\n",
		       gma, strerror ( rc ) );
		goto err_mcast_attach;
	}

	/* Initiate multicast membership join */
	if ( ( rc = ib_mc_member_request ( gma, gid, 1 ) ) != 0 )
		goto err_mc_member_record;

	return 0;

 err_mc_member_record:
	ib_mcast_detach ( ibdev, qp, gid );
 err_mcast_attach:
	return rc;
}

/**
 * Leave multicast group
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v gid		Multicast GID
 */
void ib_mcast_leave ( struct ib_device *ibdev, struct ib_queue_pair *qp,
		      struct ib_gid *gid ) {
	struct ib_gma *gma = ibdev->gma;

	DBGC ( gma, "GMA %p QPN %lx leaving %08x:%08x:%08x:%08x\n",
	       gma, qp->qpn, ntohl ( gid->u.dwords[0] ),
	       ntohl ( gid->u.dwords[1] ), ntohl ( gid->u.dwords[2] ),
	       ntohl ( gid->u.dwords[3] ) );

	/* Detach queue pair from multicast GID */
	ib_mcast_detach ( ibdev, qp, gid );

	/* Initiate multicast membership leave */
	ib_mc_member_request ( gma, gid, 0 );
}

/**
 * Handle multicast membership record join response
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @ret mad		MAD response
 */
static union ib_mad * ib_handle_mc_member_join ( struct ib_gma *gma,
						 union ib_mad *mad ) {
	struct ib_device *ibdev = gma->ibdev;
	struct ib_mc_member_record *mc_member_record =
		&mad->sa.sa_data.mc_member_record;
	struct ib_queue_pair *qp;
	struct ib_gid *gid;
	unsigned long qkey;
	int rc;

	/* Ignore if not a success */
	if ( mad->hdr.status != htons ( IB_MGMT_STATUS_OK ) ) {
		DBGC ( gma, "GMA %p join failed with status %04x\n",
		       gma, ntohs ( mad->hdr.status ) );
		return NULL;
	}

	/* Extract MAD parameters */
	gid = &mc_member_record->mgid;
	qkey = ntohl ( mc_member_record->qkey );

	/* Locate matching queue pair */
	qp = ib_find_qp_mgid ( ibdev, gid );
	if ( ! qp ) {
		DBGC ( gma, "GMA %p has no QP to join %08x:%08x:%08x:%08x\n",
		       gma, ntohl ( gid->u.dwords[0] ),
		       ntohl ( gid->u.dwords[1] ),
		       ntohl ( gid->u.dwords[2] ),
		       ntohl ( gid->u.dwords[3] ) );
		return NULL;
	}
	DBGC ( gma, "GMA %p QPN %lx joined %08x:%08x:%08x:%08x qkey %lx\n",
	       gma, qp->qpn, ntohl ( gid->u.dwords[0] ),
	       ntohl ( gid->u.dwords[1] ), ntohl ( gid->u.dwords[2] ),
	       ntohl ( gid->u.dwords[3] ), qkey );

	/* Set queue key */
	if ( ( rc = ib_modify_qp ( ibdev, qp, IB_MODIFY_QKEY, qkey ) ) != 0 ) {
		DBGC ( gma, "GMA %p QPN %lx could not modify qkey: %s\n",
		       gma, qp->qpn, strerror ( rc ) );
		return NULL;
	}

	return NULL;
}

/**
 * Handle multicast membership record leave response
 *
 * @v gma		General management agent
 * @v mad		MAD
 * @v response		MAD response
 */
static union ib_mad * ib_handle_mc_member_leave ( struct ib_gma *gma,
						  union ib_mad *mad ) {
	struct ib_mc_member_record *mc_member_record =
		&mad->sa.sa_data.mc_member_record;
	struct ib_gid *gid;

	/* Ignore if not a success */
	if ( mad->hdr.status != htons ( IB_MGMT_STATUS_OK ) ) {
		DBGC ( gma, "GMA %p leave failed with status %04x\n",
		       gma, ntohs ( mad->hdr.status ) );
		return NULL;
	}

	/* Extract MAD parameters */
	gid = &mc_member_record->mgid;
	DBGC ( gma, "GMA %p left %08x:%08x:%08x:%08x\n", gma,
	       ntohl ( gid->u.dwords[0] ), ntohl ( gid->u.dwords[1] ),
	       ntohl ( gid->u.dwords[2] ), ntohl ( gid->u.dwords[3] ) );

	return NULL;
}

/** Multicast membership record response handler */
struct ib_gma_handler ib_mc_member_record_handlers[] __ib_gma_handler = {
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_ADM,
		.class_version = IB_SA_CLASS_VERSION,
		.method = IB_MGMT_METHOD_GET_RESP,
		.attr_id = htons ( IB_SA_ATTR_MC_MEMBER_REC ),
		.handle = ib_handle_mc_member_join,
	},
	{
		.mgmt_class = IB_MGMT_CLASS_SUBN_ADM,
		.class_version = IB_SA_CLASS_VERSION,
		.method = IB_SA_METHOD_DELETE_RESP,
		.attr_id = htons ( IB_SA_ATTR_MC_MEMBER_REC ),
		.handle = ib_handle_mc_member_leave,
	},
};
