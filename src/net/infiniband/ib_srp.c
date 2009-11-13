/*
 * Copyright (C) 2009 Fen Systems Ltd <mbrown@fensystems.co.uk>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

FILE_LICENCE ( BSD2 );

#include <stdlib.h>
#include <errno.h>
#include <gpxe/srp.h>
#include <gpxe/infiniband.h>
#include <gpxe/ib_cmrc.h>
#include <gpxe/ib_srp.h>

/**
 * @file
 *
 * SCSI RDMA Protocol over Infiniband
 *
 */

/* Disambiguate the various possible EINVALs */
#define EINVAL_BYTE_STRING_LEN ( EINVAL | EUNIQ_01 )
#define EINVAL_BYTE_STRING ( EINVAL | EUNIQ_02 )
#define EINVAL_INTEGER ( EINVAL | EUNIQ_03 )
#define EINVAL_RP_TOO_SHORT ( EINVAL | EUNIQ_04 )

/** IB SRP parse flags */
enum ib_srp_parse_flags {
	IB_SRP_PARSE_REQUIRED = 0x0000,
	IB_SRP_PARSE_OPTIONAL = 0x8000,
	IB_SRP_PARSE_FLAG_MASK = 0xf000,
};

/** IB SRP root path parameters */
struct ib_srp_root_path {
	/** SCSI LUN */
	struct scsi_lun *lun;
	/** SRP port IDs */
	struct srp_port_ids *port_ids;
	/** IB SRP parameters */
	struct ib_srp_parameters *ib;
};

/**
 * Parse IB SRP root path byte-string value
 *
 * @v rp_comp		Root path component string
 * @v default_value	Default value to use if component string is empty
 * @ret value		Value
 */
static int ib_srp_parse_byte_string ( const char *rp_comp, uint8_t *bytes,
				      unsigned int size_flags ) {
	size_t size = ( size_flags & ~IB_SRP_PARSE_FLAG_MASK );
	size_t rp_comp_len = strlen ( rp_comp );
	char buf[3];
	char *buf_end;

	/* Allow optional components to be empty */
	if ( ( rp_comp_len == 0 ) &&
	     ( size_flags & IB_SRP_PARSE_OPTIONAL ) )
		return 0;

	/* Check string length */
	if ( rp_comp_len != ( 2 * size ) )
		return -EINVAL_BYTE_STRING_LEN;

	/* Parse byte string */
	for ( ; size ; size--, rp_comp += 2, bytes++ ) {
		memcpy ( buf, rp_comp, 2 );
		buf[2] = '\0';
		*bytes = strtoul ( buf, &buf_end, 16 );
		if ( buf_end != &buf[2] )
			return -EINVAL_BYTE_STRING;
	}
	return 0;
}

/**
 * Parse IB SRP root path integer value
 *
 * @v rp_comp		Root path component string
 * @v default_value	Default value to use if component string is empty
 * @ret value		Value
 */
static int ib_srp_parse_integer ( const char *rp_comp, int default_value ) {
	int value;
	char *end;

	value = strtoul ( rp_comp, &end, 16 );
	if ( *end )
		return -EINVAL_INTEGER;

	if ( end == rp_comp )
		return default_value;

	return value;
}

/**
 * Parse IB SRP root path literal component
 *
 * @v rp_comp		Root path component string
 * @v rp		IB SRP root path
 * @ret rc		Return status code
 */
static int ib_srp_parse_literal ( const char *rp_comp __unused,
				  struct ib_srp_root_path *rp __unused ) {
	/* Ignore */
	return 0;
}

/**
 * Parse IB SRP root path source GID
 *
 * @v rp_comp		Root path component string
 * @v rp		IB SRP root path
 * @ret rc		Return status code
 */
static int ib_srp_parse_sgid ( const char *rp_comp,
			       struct ib_srp_root_path *rp ) {
	struct ib_device *ibdev;

	/* Default to the GID of the last opened Infiniband device */
	if ( ( ibdev = last_opened_ibdev() ) != NULL )
		memcpy ( &rp->ib->sgid, &ibdev->gid, sizeof ( rp->ib->sgid ) );

	return ib_srp_parse_byte_string ( rp_comp, rp->ib->sgid.u.bytes,
					  ( sizeof ( rp->ib->sgid ) |
					    IB_SRP_PARSE_OPTIONAL ) );
}

/**
 * Parse IB SRP root path initiator identifier extension
 *
 * @v rp_comp		Root path component string
 * @v rp		IB SRP root path
 * @ret rc		Return status code
 */
static int ib_srp_parse_initiator_id_ext ( const char *rp_comp,
					   struct ib_srp_root_path *rp ) {
	struct ib_srp_initiator_port_id *port_id =
		ib_srp_initiator_port_id ( rp->port_ids );

	return ib_srp_parse_byte_string ( rp_comp, port_id->id_ext.u.bytes,
					  ( sizeof ( port_id->id_ext ) |
					    IB_SRP_PARSE_OPTIONAL ) );
}

/**
 * Parse IB SRP root path initiator HCA GUID
 *
 * @v rp_comp		Root path component string
 * @v rp		IB SRP root path
 * @ret rc		Return status code
 */
static int ib_srp_parse_initiator_hca_guid ( const char *rp_comp,
					     struct ib_srp_root_path *rp ) {
	struct ib_srp_initiator_port_id *port_id =
		ib_srp_initiator_port_id ( rp->port_ids );

	/* Default to the GUID portion of the source GID */
	memcpy ( &port_id->hca_guid, &rp->ib->sgid.u.half[1],
		 sizeof ( port_id->hca_guid ) );

	return ib_srp_parse_byte_string ( rp_comp, port_id->hca_guid.u.bytes,
					  ( sizeof ( port_id->hca_guid ) |
					    IB_SRP_PARSE_OPTIONAL ) );
}

/**
 * Parse IB SRP root path destination GID
 *
 * @v rp_comp		Root path component string
 * @v rp		IB SRP root path
 * @ret rc		Return status code
 */
static int ib_srp_parse_dgid ( const char *rp_comp,
			       struct ib_srp_root_path *rp ) {
	return ib_srp_parse_byte_string ( rp_comp, rp->ib->dgid.u.bytes,
					  ( sizeof ( rp->ib->dgid ) |
					    IB_SRP_PARSE_REQUIRED ) );
}

/**
 * Parse IB SRP root path partition key
 *
 * @v rp_comp		Root path component string
 * @v rp		IB SRP root path
 * @ret rc		Return status code
 */
static int ib_srp_parse_pkey ( const char *rp_comp,
			       struct ib_srp_root_path *rp ) {
	int pkey;

	if ( ( pkey = ib_srp_parse_integer ( rp_comp, IB_PKEY_DEFAULT ) ) < 0 )
		return pkey;
	rp->ib->pkey = pkey;
	return 0;
}

/**
 * Parse IB SRP root path service ID
 *
 * @v rp_comp		Root path component string
 * @v rp		IB SRP root path
 * @ret rc		Return status code
 */
static int ib_srp_parse_service_id ( const char *rp_comp,
				     struct ib_srp_root_path *rp ) {
	return ib_srp_parse_byte_string ( rp_comp, rp->ib->service_id.u.bytes,
					  ( sizeof ( rp->ib->service_id ) |
					    IB_SRP_PARSE_REQUIRED ) );
}

/**
 * Parse IB SRP root path LUN
 *
 * @v rp_comp		Root path component string
 * @v rp		IB SRP root path
 * @ret rc		Return status code
 */
static int ib_srp_parse_lun ( const char *rp_comp,
			      struct ib_srp_root_path *rp ) {
	return scsi_parse_lun ( rp_comp, rp->lun );
}

/**
 * Parse IB SRP root path target identifier extension
 *
 * @v rp_comp		Root path component string
 * @v rp		IB SRP root path
 * @ret rc		Return status code
 */
static int ib_srp_parse_target_id_ext ( const char *rp_comp,
					struct ib_srp_root_path *rp ) {
	struct ib_srp_target_port_id *port_id =
		ib_srp_target_port_id ( rp->port_ids );

	return ib_srp_parse_byte_string ( rp_comp, port_id->id_ext.u.bytes,
					  ( sizeof ( port_id->id_ext ) |
					    IB_SRP_PARSE_REQUIRED ) );
}

/**
 * Parse IB SRP root path target I/O controller GUID
 *
 * @v rp_comp		Root path component string
 * @v rp		IB SRP root path
 * @ret rc		Return status code
 */
static int ib_srp_parse_target_ioc_guid ( const char *rp_comp,
					  struct ib_srp_root_path *rp ) {
	struct ib_srp_target_port_id *port_id =
		ib_srp_target_port_id ( rp->port_ids );

	return ib_srp_parse_byte_string ( rp_comp, port_id->ioc_guid.u.bytes,
					  ( sizeof ( port_id->ioc_guid ) |
					    IB_SRP_PARSE_REQUIRED ) );
}

/** IB SRP root path component parser */
struct ib_srp_root_path_parser {
	/**
	 * Parse IB SRP root path component
	 *
	 * @v rp_comp		Root path component string
	 * @v rp		IB SRP root path
	 * @ret rc		Return status code
	 */
	int ( * parse ) ( const char *rp_comp, struct ib_srp_root_path *rp );
};

/** IB SRP root path components */
static struct ib_srp_root_path_parser ib_srp_rp_parser[] = {
	{ ib_srp_parse_literal },
	{ ib_srp_parse_sgid },
	{ ib_srp_parse_initiator_id_ext },
	{ ib_srp_parse_initiator_hca_guid },
	{ ib_srp_parse_dgid },
	{ ib_srp_parse_pkey },
	{ ib_srp_parse_service_id },
	{ ib_srp_parse_lun },
	{ ib_srp_parse_target_id_ext },
	{ ib_srp_parse_target_ioc_guid },
};

/** Number of IB SRP root path components */
#define IB_SRP_NUM_RP_COMPONENTS \
	( sizeof ( ib_srp_rp_parser ) / sizeof ( ib_srp_rp_parser[0] ) )

/**
 * Parse IB SRP root path
 *
 * @v srp		SRP device
 * @v rp_string		Root path
 * @ret rc		Return status code
 */
static int ib_srp_parse_root_path ( struct srp_device *srp,
				    const char *rp_string ) {
	struct ib_srp_parameters *ib_params = ib_srp_params ( srp );
	struct ib_srp_root_path rp = {
		.lun = &srp->lun,
		.port_ids = &srp->port_ids,
		.ib = ib_params,
	};
	char rp_string_copy[ strlen ( rp_string ) + 1 ];
	char *rp_comp[IB_SRP_NUM_RP_COMPONENTS];
	char *rp_string_tmp = rp_string_copy;
	unsigned int i = 0;
	int rc;

	/* Split root path into component parts */
	strcpy ( rp_string_copy, rp_string );
	while ( 1 ) {
		rp_comp[i++] = rp_string_tmp;
		if ( i == IB_SRP_NUM_RP_COMPONENTS )
			break;
		for ( ; *rp_string_tmp != ':' ; rp_string_tmp++ ) {
			if ( ! *rp_string_tmp ) {
				DBGC ( srp, "SRP %p root path \"%s\" too "
				       "short\n", srp, rp_string );
				return -EINVAL_RP_TOO_SHORT;
			}
		}
		*(rp_string_tmp++) = '\0';
	}

	/* Parse root path components */
	for ( i = 0 ; i < IB_SRP_NUM_RP_COMPONENTS ; i++ ) {
		if ( ( rc = ib_srp_rp_parser[i].parse ( rp_comp[i],
							&rp ) ) != 0 ) {
			DBGC ( srp, "SRP %p could not parse \"%s\" in root "
			       "path \"%s\": %s\n", srp, rp_comp[i],
			       rp_string, strerror ( rc ) );
			return rc;
		}
	}

	return 0;
}

/**
 * Connect IB SRP session
 *
 * @v srp		SRP device
 * @ret rc		Return status code
 */
static int ib_srp_connect ( struct srp_device *srp ) {
	struct ib_srp_parameters *ib_params = ib_srp_params ( srp );
	struct ib_device *ibdev;
	int rc;

	/* Identify Infiniband device */
	ibdev = find_ibdev ( &ib_params->sgid );
	if ( ! ibdev ) {
		DBGC ( srp, "SRP %p could not identify Infiniband device\n",
		       srp );
		return -ENODEV;
	}

	/* Configure remaining SRP parameters */
	srp->memory_handle = ibdev->rdma_key;

	/* Open CMRC socket */
	if ( ( rc = ib_cmrc_open ( &srp->socket, ibdev, &ib_params->dgid,
				   &ib_params->service_id ) ) != 0 ) {
		DBGC ( srp, "SRP %p could not open CMRC socket: %s\n",
		       srp, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/** IB SRP transport type */
struct srp_transport_type ib_srp_transport = {
	.priv_len = sizeof ( struct ib_srp_parameters ),
	.parse_root_path = ib_srp_parse_root_path,
	.connect = ib_srp_connect,
};
