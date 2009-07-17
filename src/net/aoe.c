/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <gpxe/list.h>
#include <gpxe/if_ether.h>
#include <gpxe/ethernet.h>
#include <gpxe/iobuf.h>
#include <gpxe/uaccess.h>
#include <gpxe/ata.h>
#include <gpxe/netdevice.h>
#include <gpxe/process.h>
#include <gpxe/features.h>
#include <gpxe/aoe.h>

/** @file
 *
 * AoE protocol
 *
 */

FEATURE ( FEATURE_PROTOCOL, "AoE", DHCP_EB_FEATURE_AOE, 1 );

struct net_protocol aoe_protocol;

/** List of all AoE sessions */
static LIST_HEAD ( aoe_sessions );

static void aoe_free ( struct refcnt *refcnt ) {
	struct aoe_session *aoe =
		container_of ( refcnt, struct aoe_session, refcnt );

	netdev_put ( aoe->netdev );
	free ( aoe );
}

/**
 * Mark current AoE command complete
 *
 * @v aoe		AoE session
 * @v rc		Return status code
 */
static void aoe_done ( struct aoe_session *aoe, int rc ) {

	/* Record overall command status */
	if ( aoe->command ) {
		aoe->command->cb.cmd_stat = aoe->status;
		aoe->command->rc = rc;
		aoe->command = NULL;
	}

	/* Stop retransmission timer */
	stop_timer ( &aoe->timer );

	/* Mark operation as complete */
	aoe->rc = rc;
}

/**
 * Send AoE command
 *
 * @v aoe		AoE session
 * @ret rc		Return status code
 *
 * This transmits an AoE command packet.  It does not wait for a
 * response.
 */
static int aoe_send_command ( struct aoe_session *aoe ) {
	struct ata_command *command = aoe->command;
	struct io_buffer *iobuf;
	struct aoehdr *aoehdr;
	union aoecmd *aoecmd;
	struct aoeata *aoeata;
	unsigned int count;
	unsigned int data_out_len;
	unsigned int aoecmdlen;

	/* Fail immediately if we have no netdev to send on */
	if ( ! aoe->netdev ) {
		aoe_done ( aoe, -ENETUNREACH );
		return -ENETUNREACH;
	}

	/* If we are transmitting anything that requires a response,
         * start the retransmission timer.  Do this before attempting
         * to allocate the I/O buffer, in case allocation itself
         * fails.
         */
	start_timer ( &aoe->timer );

	/* Calculate count and data_out_len for this subcommand */
	switch ( aoe->aoe_cmd_type ) {
	case AOE_CMD_ATA:
		count = command->cb.count.native;
		if ( count > AOE_MAX_COUNT )
			count = AOE_MAX_COUNT;
		data_out_len = ( command->data_out ?
				 ( count * ATA_SECTOR_SIZE ) : 0 );
		aoecmdlen = sizeof ( aoecmd->ata );
		break;
	case AOE_CMD_CONFIG:
		count = 0;
		data_out_len = 0;
		aoecmdlen = sizeof ( aoecmd->cfg );
		break;
	default:
		return -ENOTSUP;
	}

	/* Create outgoing I/O buffer */
	iobuf = alloc_iob ( ETH_HLEN + sizeof ( *aoehdr ) +
			    aoecmdlen + data_out_len );

	if ( ! iobuf )
		return -ENOMEM;
	iob_reserve ( iobuf, ETH_HLEN );
	aoehdr = iob_put ( iobuf, sizeof ( *aoehdr ) );
	aoecmd = iob_put ( iobuf, aoecmdlen );
	memset ( aoehdr, 0, ( sizeof ( *aoehdr ) + aoecmdlen ) );

	/* Fill AoE header */
	aoehdr->ver_flags = AOE_VERSION;
	aoehdr->major = htons ( aoe->major );
	aoehdr->minor = aoe->minor;
	aoehdr->command = aoe->aoe_cmd_type;
	aoehdr->tag = htonl ( ++aoe->tag );

	/* Fill AoE payload */
	switch ( aoe->aoe_cmd_type ) {
	case AOE_CMD_ATA:
		/* Fill AoE command */
		aoeata = &aoecmd->ata;
		linker_assert ( AOE_FL_DEV_HEAD	== ATA_DEV_SLAVE,
				__fix_ata_h__ );
		aoeata->aflags = ( ( command->cb.lba48 ? AOE_FL_EXTENDED : 0 )|
				   ( command->cb.device & ATA_DEV_SLAVE ) |
				   ( data_out_len ? AOE_FL_WRITE : 0 ) );
		aoeata->err_feat = command->cb.err_feat.bytes.cur;
		aoeata->count = count;
		aoeata->cmd_stat = command->cb.cmd_stat;
		aoeata->lba.u64 = cpu_to_le64 ( command->cb.lba.native );
		if ( ! command->cb.lba48 )
			aoeata->lba.bytes[3] |=
				( command->cb.device & ATA_DEV_MASK );

		/* Fill data payload */
		copy_from_user ( iob_put ( iobuf, data_out_len ),
				 command->data_out, aoe->command_offset,
				 data_out_len );
		break;
	case AOE_CMD_CONFIG:
		/* Nothing to do */
		break;
	default:
		assert ( 0 );
	}

	/* Send packet */
	return net_tx ( iobuf, aoe->netdev, &aoe_protocol, aoe->target );
}

/**
 * Handle AoE retry timer expiry
 *
 * @v timer		AoE retry timer
 * @v fail		Failure indicator
 */
static void aoe_timer_expired ( struct retry_timer *timer, int fail ) {
	struct aoe_session *aoe =
		container_of ( timer, struct aoe_session, timer );

	if ( fail ) {
		aoe_done ( aoe, -ETIMEDOUT );
	} else {
		aoe_send_command ( aoe );
	}
}

/**
 * Handle AoE configuration command response
 *
 * @v aoe		AoE session
 * @v ll_source		Link-layer source address
 * @ret rc		Return status code
 */
static int aoe_rx_cfg ( struct aoe_session *aoe, const void *ll_source ) {

	/* Record target MAC address */
	memcpy ( aoe->target, ll_source, sizeof ( aoe->target ) );
	DBGC ( aoe, "AoE %p target MAC address %s\n",
	       aoe, eth_ntoa ( aoe->target ) );

	/* Mark config request as complete */
	aoe_done ( aoe, 0 );

	return 0;
}

/**
 * Handle AoE ATA command response
 *
 * @v aoe		AoE session
 * @v aoeata		AoE ATA command
 * @v len		Length of AoE ATA command
 * @ret rc		Return status code
 */
static int aoe_rx_ata ( struct aoe_session *aoe, struct aoeata *aoeata,
			size_t len ) {
	struct ata_command *command = aoe->command;
	unsigned int rx_data_len;
	unsigned int count;
	unsigned int data_len;

	/* Sanity check */
	if ( len < sizeof ( *aoeata ) ) {
		/* Ignore packet; allow timer to trigger retransmit */
		return -EINVAL;
	}
	rx_data_len = ( len - sizeof ( *aoeata ) );

	/* Calculate count and data_len for this subcommand */
	count = command->cb.count.native;
	if ( count > AOE_MAX_COUNT )
		count = AOE_MAX_COUNT;
	data_len = count * ATA_SECTOR_SIZE;

	/* Merge into overall ATA status */
	aoe->status |= aoeata->cmd_stat;

	/* Copy data payload */
	if ( command->data_in ) {
		if ( rx_data_len > data_len )
			rx_data_len = data_len;
		copy_to_user ( command->data_in, aoe->command_offset,
			       aoeata->data, rx_data_len );
	}

	/* Update ATA command and offset */
	aoe->command_offset += data_len;
	command->cb.lba.native += count;
	command->cb.count.native -= count;

	/* Check for operation complete */
	if ( ! command->cb.count.native ) {
		aoe_done ( aoe, 0 );
		return 0;
	}

	/* Transmit next portion of request */
	stop_timer ( &aoe->timer );
	aoe_send_command ( aoe );

	return 0;
}

/**
 * Process incoming AoE packets
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v ll_source		Link-layer source address
 * @ret rc		Return status code
 *
 */
static int aoe_rx ( struct io_buffer *iobuf,
		    struct net_device *netdev __unused,
		    const void *ll_source ) {
	struct aoehdr *aoehdr = iobuf->data;
	struct aoe_session *aoe;
	int rc = 0;

	/* Sanity checks */
	if ( iob_len ( iobuf ) < sizeof ( *aoehdr ) ) {
		rc = -EINVAL;
		goto done;
	}
	if ( ( aoehdr->ver_flags & AOE_VERSION_MASK ) != AOE_VERSION ) {
		rc = -EPROTONOSUPPORT;
		goto done;
	}
	if ( ! ( aoehdr->ver_flags & AOE_FL_RESPONSE ) ) {
		/* Ignore AoE requests that we happen to see */
		goto done;
	}
	iob_pull ( iobuf, sizeof ( *aoehdr ) );

	/* Demultiplex amongst active AoE sessions */
	list_for_each_entry ( aoe, &aoe_sessions, list ) {
		if ( ntohs ( aoehdr->major ) != aoe->major )
			continue;
		if ( aoehdr->minor != aoe->minor )
			continue;
		if ( ntohl ( aoehdr->tag ) != aoe->tag )
			continue;
		if ( aoehdr->ver_flags & AOE_FL_ERROR ) {
			aoe_done ( aoe, -EIO );
			break;
		}
		switch ( aoehdr->command ) {
		case AOE_CMD_ATA:
			rc = aoe_rx_ata ( aoe, iobuf->data, iob_len ( iobuf ));
			break;
		case AOE_CMD_CONFIG:
			rc = aoe_rx_cfg ( aoe, ll_source );
			break;
		default:
			DBGC ( aoe, "AoE %p ignoring command %02x\n",
			       aoe, aoehdr->command );
			break;
		}
		break;
	}

 done:
	free_iob ( iobuf );
	return rc;
}

/** AoE protocol */
struct net_protocol aoe_protocol __net_protocol = {
	.name = "AoE",
	.net_proto = htons ( ETH_P_AOE ),
	.rx = aoe_rx,
};

/**
 * Issue ATA command via an open AoE session
 *
 * @v ata		ATA device
 * @v command		ATA command
 * @ret rc		Return status code
 */
static int aoe_command ( struct ata_device *ata,
			 struct ata_command *command ) {
	struct aoe_session *aoe =
		container_of ( ata->backend, struct aoe_session, refcnt );

	aoe->command = command;
	aoe->status = 0;
	aoe->command_offset = 0;
	aoe->aoe_cmd_type = AOE_CMD_ATA;

	aoe_send_command ( aoe );

	return 0;
}

/**
 * Issue AoE config query for AoE target discovery
 *
 * @v aoe		AoE session
 * @ret rc		Return status code
 */
static int aoe_discover ( struct aoe_session *aoe ) {
	int rc;

	aoe->status = 0;
	aoe->aoe_cmd_type = AOE_CMD_CONFIG;
	aoe->command = NULL;

	aoe_send_command ( aoe );

	aoe->rc = -EINPROGRESS;
	while ( aoe->rc == -EINPROGRESS )
		step();
	rc = aoe->rc;

	return rc;
}

static int aoe_detached_command ( struct ata_device *ata __unused,
				  struct ata_command *command __unused ) {
	return -ENODEV;
}

void aoe_detach ( struct ata_device *ata ) {
	struct aoe_session *aoe =
		container_of ( ata->backend, struct aoe_session, refcnt );

	stop_timer ( &aoe->timer );
	ata->command = aoe_detached_command;
	list_del ( &aoe->list );
	ref_put ( ata->backend );
	ata->backend = NULL;
}

static int aoe_parse_root_path ( struct aoe_session *aoe,
				 const char *root_path ) {
	char *ptr;

	if ( strncmp ( root_path, "aoe:", 4 ) != 0 )
		return -EINVAL;
	ptr = ( ( char * ) root_path + 4 );

	if ( *ptr++ != 'e' )
		return -EINVAL;

	aoe->major = strtoul ( ptr, &ptr, 10 );
	if ( *ptr++ != '.' )
		return -EINVAL;

	aoe->minor = strtoul ( ptr, &ptr, 10 );
	if ( *ptr )
		return -EINVAL;

	return 0;
}

int aoe_attach ( struct ata_device *ata, struct net_device *netdev,
		 const char *root_path ) {
	struct aoe_session *aoe;
	int rc;

	/* Allocate and initialise structure */
	aoe = zalloc ( sizeof ( *aoe ) );
	if ( ! aoe )
		return -ENOMEM;
	aoe->refcnt.free = aoe_free;
	aoe->netdev = netdev_get ( netdev );
	memcpy ( aoe->target, netdev->ll_broadcast, sizeof ( aoe->target ) );
	aoe->tag = AOE_TAG_MAGIC;
	aoe->timer.expired = aoe_timer_expired;

	/* Parse root path */
	if ( ( rc = aoe_parse_root_path ( aoe, root_path ) ) != 0 )
		goto err;

	/* Attach parent interface, transfer reference to connection
	 * list, and return
	 */
	ata->backend = ref_get ( &aoe->refcnt );
	ata->command = aoe_command;
	list_add ( &aoe->list, &aoe_sessions );

	/* Send discovery packet to find the target MAC address.
	 * Ideally, this ought to be done asynchronously, but the
	 * block device interface does not yet support asynchronous
	 * operation.
	 */
	if ( ( rc = aoe_discover( aoe ) ) != 0 )
	       goto err;

	return 0;

 err:
	ref_put ( &aoe->refcnt );
	return rc;
}
