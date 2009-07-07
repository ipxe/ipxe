/*
 * Copyright (C) 2009 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <gpxe/infiniband.h>
#include <gpxe/ib_gma.h>
#include <gpxe/ib_pathrec.h>

/** @file
 *
 * Infiniband path lookups
 *
 */

/** Number of path record cache entries
 *
 * Must be a power of two.
 */
#define IB_NUM_CACHED_PATHS 4

/** A path record cache entry */
struct ib_cached_path_record {
	/** Infiniband device's port GID
	 *
	 * Used to disambiguate cache entries when we have multiple
	 * Infiniband devices, without having to maintain a pointer to
	 * the Infiniband device.
	 */
	struct ib_gid sgid;
	/** Destination GID */
	struct ib_gid dgid;
	/** Destination LID */
	unsigned int dlid;
	/** Rate */
	unsigned int rate;
	/** Service level */
	unsigned int sl;
};

/** Path record cache */
static struct ib_cached_path_record ib_path_cache[IB_NUM_CACHED_PATHS];

/** Oldest path record cache entry index */
static unsigned int ib_path_cache_idx;

/**
 * Find path record cache entry
 *
 * @v ibdev		Infiniband device
 * @v dgid		Destination GID
 * @ret cached		Path record cache entry, or NULL
 */
static struct ib_cached_path_record *
ib_find_path_cache_entry ( struct ib_device *ibdev, struct ib_gid *dgid ) {
	struct ib_cached_path_record *cached;
	unsigned int i;

	for ( i = 0 ; i < IB_NUM_CACHED_PATHS ; i++ ) {
		cached = &ib_path_cache[i];
		if ( memcmp ( &cached->sgid, &ibdev->gid,
			      sizeof ( cached->sgid ) ) != 0 )
			continue;
		if ( memcmp ( &cached->dgid, dgid,
			      sizeof ( cached->dgid ) ) != 0 )
			continue;
		return cached;
	}

	return NULL;
}

/**
 * Resolve path record
 *
 * @v ibdev		Infiniband device
 * @v av		Address vector to complete
 * @ret rc		Return status code
 */
int ib_resolve_path ( struct ib_device *ibdev,
		      struct ib_address_vector *av ) {
	struct ib_gid *gid = &av->gid;
	struct ib_cached_path_record *cached;
	union ib_mad mad;
	struct ib_mad_sa *sa = &mad.sa;
	unsigned int cache_idx;
	int rc;

	/* Sanity check */
	if ( ! av->gid_present ) {
		DBGC ( ibdev, "IBDEV %p attempt to look up path record "
		       "without GID\n", ibdev );
		return -EINVAL;
	}

	/* Look in cache for a matching entry */
	cached = ib_find_path_cache_entry ( ibdev, gid );
	if ( cached && cached->dlid ) {
		/* Populated entry found */
		av->lid = cached->dlid;
		av->rate = cached->rate;
		av->sl = cached->sl;
		DBGC2 ( ibdev, "IBDEV %p cache hit for %08x:%08x:%08x:%08x\n",
			ibdev, htonl ( gid->u.dwords[0] ),
			htonl ( gid->u.dwords[1] ), htonl ( gid->u.dwords[2] ),
			htonl ( gid->u.dwords[3] ) );
		return 0;
	}
	DBGC ( ibdev, "IBDEV %p cache miss for %08x:%08x:%08x:%08x%s\n", ibdev,
	       htonl ( gid->u.dwords[0] ), htonl ( gid->u.dwords[1] ),
	       htonl ( gid->u.dwords[2] ), htonl ( gid->u.dwords[3] ),
	       ( cached ? " (in progress)" : "" ) );

	/* If no unresolved entry was found, then create a new one */
	if ( ! cached ) {
		cache_idx = ( (ib_path_cache_idx++) % IB_NUM_CACHED_PATHS );
		cached = &ib_path_cache[cache_idx];
		memset ( cached, 0, sizeof ( *cached ) );
		memcpy ( &cached->sgid, &ibdev->gid, sizeof ( cached->sgid ) );
		memcpy ( &cached->dgid, gid, sizeof ( cached->dgid ) );
	}

	/* Construct path record request */
	memset ( sa, 0, sizeof ( *sa ) );
	sa->mad_hdr.base_version = IB_MGMT_BASE_VERSION;
	sa->mad_hdr.mgmt_class = IB_MGMT_CLASS_SUBN_ADM;
	sa->mad_hdr.class_version = IB_SA_CLASS_VERSION;
	sa->mad_hdr.method = IB_MGMT_METHOD_GET;
	sa->mad_hdr.attr_id = htons ( IB_SA_ATTR_PATH_REC );
	sa->sa_hdr.comp_mask[1] =
		htonl ( IB_SA_PATH_REC_DGID | IB_SA_PATH_REC_SGID );
	memcpy ( &sa->sa_data.path_record.dgid, &cached->dgid,
		 sizeof ( sa->sa_data.path_record.dgid ) );
	memcpy ( &sa->sa_data.path_record.sgid, &cached->sgid,
		 sizeof ( sa->sa_data.path_record.sgid ) );

	/* Issue path record request */
	if ( ( rc = ib_gma_request ( &ibdev->gma, &mad, NULL ) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not get path record: %s\n",
		       ibdev, strerror ( rc ) );
		return rc;
	}

	/* Not found yet */
	return -ENOENT;
}

/**
 * Handle path record response
 *
 * @v ibdev		Infiniband device
 * @v mad		MAD
 * @ret rc		Return status code
 */
static int ib_handle_path_record ( struct ib_device *ibdev,
				   union ib_mad *mad ) {
	struct ib_path_record *path_record = &mad->sa.sa_data.path_record;
	struct ib_gid *dgid = &path_record->dgid;
	struct ib_cached_path_record *cached;
	unsigned int dlid;
	unsigned int sl;
	unsigned int rate;

	/* Ignore if not a success */
	if ( mad->hdr.status != htons ( IB_MGMT_STATUS_OK ) ) {
		DBGC ( ibdev, "IBDEV %p path record lookup failed with status "
		       "%04x\n", ibdev, ntohs ( mad->hdr.status ) );
		return -EINVAL;
	}

	/* Extract values from MAD */
	dlid = ntohs ( path_record->dlid );
	sl = ( path_record->reserved__sl & 0x0f );
	rate = ( path_record->rate_selector__rate & 0x3f );
	DBGC ( ibdev, "IBDEV %p path to %08x:%08x:%08x:%08x is %04x sl %d "
	       "rate %d\n", ibdev, htonl ( dgid->u.dwords[0] ),
	       htonl ( dgid->u.dwords[1] ), htonl ( dgid->u.dwords[2] ),
	       htonl ( dgid->u.dwords[3] ), dlid, sl, rate );

	/* Look for a matching cache entry to fill in */
	if ( ( cached = ib_find_path_cache_entry ( ibdev, dgid ) ) != NULL ) {
		DBGC ( ibdev, "IBDEV %p cache add for %08x:%08x:%08x:%08x\n",
		       ibdev, htonl ( dgid->u.dwords[0] ),
		       htonl ( dgid->u.dwords[1] ),
		       htonl ( dgid->u.dwords[2] ),
		       htonl ( dgid->u.dwords[3] ) );
		cached->dlid = dlid;
		cached->rate = rate;
		cached->sl = sl;
	}

	return 0;
}

/** Path record response handler */
struct ib_mad_handler ib_path_record_handler __ib_mad_handler = {
	.mgmt_class = IB_MGMT_CLASS_SUBN_ADM,
	.class_version = IB_SA_CLASS_VERSION,
	.method = IB_MGMT_METHOD_GET_RESP,
	.attr_id = htons ( IB_SA_ATTR_PATH_REC ),
	.handle = ib_handle_path_record,
};
