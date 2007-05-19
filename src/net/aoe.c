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

#include <stddef.h>
#include <string.h>
#include <stdio.h>
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
#include <gpxe/async.h>
#include <gpxe/aoe.h>

/** @file
 *
 * AoE protocol
 *
 */

struct net_protocol aoe_protocol;

/** List of all AoE sessions */
static LIST_HEAD ( aoe_sessions );

/**
 * Mark current AoE command complete
 *
 * @v aoe		AoE session
 * @v rc		Return status code
 */
static void aoe_done ( struct aoe_session *aoe, int rc ) {

	/* Record overall command status */
	aoe->command->cb.cmd_stat = aoe->status;
	aoe->command = NULL;

	/* Mark async operation as complete */
	async_done ( &aoe->async, rc );
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
	struct aoecmd *aoecmd;
	unsigned int count;
	unsigned int data_out_len;

	/* Fail immediately if we have no netdev to send on */
	if ( ! aoe->netdev ) {
		aoe_done ( aoe, -ENETUNREACH );
		return -ENETUNREACH;
	}

	/* Calculate count and data_out_len for this subcommand */
	count = command->cb.count.native;
	if ( count > AOE_MAX_COUNT )
		count = AOE_MAX_COUNT;
	data_out_len = ( command->data_out ? ( count * ATA_SECTOR_SIZE ) : 0 );

	/* Create outgoing I/O buffer */
	iobuf = alloc_iob ( ETH_HLEN + sizeof ( *aoehdr ) + sizeof ( *aoecmd ) +
			  data_out_len );
	if ( ! iobuf )
		return -ENOMEM;
	iob_reserve ( iobuf, ETH_HLEN );
	aoehdr = iob_put ( iobuf, sizeof ( *aoehdr ) );
	aoecmd = iob_put ( iobuf, sizeof ( *aoecmd ) );
	memset ( aoehdr, 0, ( sizeof ( *aoehdr ) + sizeof ( *aoecmd ) ) );

	/* Fill AoE header */
	aoehdr->ver_flags = AOE_VERSION;
	aoehdr->major = htons ( aoe->major );
	aoehdr->minor = aoe->minor;
	aoehdr->tag = htonl ( ++aoe->tag );

	/* Fill AoE command */
	linker_assert ( AOE_FL_DEV_HEAD	== ATA_DEV_SLAVE, __fix_ata_h__ );
	aoecmd->aflags = ( ( command->cb.lba48 ? AOE_FL_EXTENDED : 0 ) |
			   ( command->cb.device & ATA_DEV_SLAVE ) |
			   ( data_out_len ? AOE_FL_WRITE : 0 ) );
	aoecmd->err_feat = command->cb.err_feat.bytes.cur;
	aoecmd->count = count;
	aoecmd->cmd_stat = command->cb.cmd_stat;
	aoecmd->lba.u64 = cpu_to_le64 ( command->cb.lba.native );
	if ( ! command->cb.lba48 )
		aoecmd->lba.bytes[3] |= ( command->cb.device & ATA_DEV_MASK );

	/* Fill data payload */
	copy_from_user ( iob_put ( iobuf, data_out_len ), command->data_out,
			 aoe->command_offset, data_out_len );

	/* Send packet */
	start_timer ( &aoe->timer );
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
 * Handle AoE response
 *
 * @v aoe		AoE session
 * @v aoehdr		AoE header
 * @ret rc		Return status code
 */
static int aoe_rx_response ( struct aoe_session *aoe, struct aoehdr *aoehdr,
			     unsigned int len ) {
	struct aoecmd *aoecmd = aoehdr->arg.command;
	struct ata_command *command = aoe->command;
	unsigned int rx_data_len;
	unsigned int count;
	unsigned int data_len;
	
	/* Sanity check */
	if ( len < ( sizeof ( *aoehdr ) + sizeof ( *aoecmd ) ) ) {
		/* Ignore packet; allow timer to trigger retransmit */
		return -EINVAL;
	}
	rx_data_len = ( len - sizeof ( *aoehdr ) - sizeof ( *aoecmd ) );

	/* Stop retry timer.  After this point, every code path must
	 * either terminate the AoE operation via aoe_done(), or
	 * transmit a new packet.
	 */
	stop_timer ( &aoe->timer );

	/* Check for fatal errors */
	if ( aoehdr->ver_flags & AOE_FL_ERROR ) {
		aoe_done ( aoe, -EIO );
		return 0;
	}

	/* Calculate count and data_len for this subcommand */
	count = command->cb.count.native;
	if ( count > AOE_MAX_COUNT )
		count = AOE_MAX_COUNT;
	data_len = count * ATA_SECTOR_SIZE;

	/* Merge into overall ATA status */
	aoe->status |= aoecmd->cmd_stat;

	/* Copy data payload */
	if ( command->data_in ) {
		if ( rx_data_len > data_len )
			rx_data_len = data_len;
		copy_to_user ( command->data_in, aoe->command_offset,
			       aoecmd->data, rx_data_len );
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
static int aoe_rx ( struct io_buffer *iobuf, struct net_device *netdev __unused,
		    const void *ll_source ) {
	struct aoehdr *aoehdr = iobuf->data;
	unsigned int len = iob_len ( iobuf );
	struct aoe_session *aoe;
	int rc = 0;

	/* Sanity checks */
	if ( len < sizeof ( *aoehdr ) ) {
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

	/* Demultiplex amongst active AoE sessions */
	list_for_each_entry ( aoe, &aoe_sessions, list ) {
		if ( ntohs ( aoehdr->major ) != aoe->major )
			continue;
		if ( aoehdr->minor != aoe->minor )
			continue;
		if ( ntohl ( aoehdr->tag ) != aoe->tag )
			continue;
		memcpy ( aoe->target, ll_source, sizeof ( aoe->target ) );
		rc = aoe_rx_response ( aoe, aoehdr, len );
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
 * Forget reference to net_device
 *
 * @v ref		Persistent reference
 */
static void aoe_forget_netdev ( struct reference *ref ) {
	struct aoe_session *aoe
		= container_of ( ref, struct aoe_session, netdev_ref );

	aoe->netdev = NULL;
	ref_del ( &aoe->netdev_ref );
}

/**
 * Open AoE session
 *
 * @v aoe		AoE session
 */
void aoe_open ( struct aoe_session *aoe ) {
	memcpy ( aoe->target, ethernet_protocol.ll_broadcast,
		 sizeof ( aoe->target ) );
	aoe->tag = AOE_TAG_MAGIC;
	aoe->timer.expired = aoe_timer_expired;
	aoe->netdev_ref.forget = aoe_forget_netdev;
	ref_add ( &aoe->netdev_ref, &aoe->netdev->references );
	list_add ( &aoe->list, &aoe_sessions );
}

/**
 * Close AoE session
 *
 * @v aoe		AoE session
 */
void aoe_close ( struct aoe_session *aoe ) {
	if ( aoe->netdev )
		ref_del ( &aoe->netdev_ref );
	list_del ( &aoe->list );
}

/**
 * Issue ATA command via an open AoE session
 *
 * @v aoe		AoE session
 * @v command		ATA command
 * @v parent		Parent asynchronous operation
 * @ret rc		Return status code
 *
 * Only one command may be issued concurrently per session.  This call
 * is non-blocking; use async_wait() to wait for the command to
 * complete.
 */
int aoe_issue ( struct aoe_session *aoe, struct ata_command *command,
		struct async *parent ) {
	aoe->command = command;
	aoe->status = 0;
	aoe->command_offset = 0;
	aoe_send_command ( aoe );
	async_init ( &aoe->async, &default_async_operations, parent );
	return 0;
}
