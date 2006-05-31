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
#include <vsprintf.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <gpxe/list.h>
#include <gpxe/if_ether.h>
#include <gpxe/pkbuff.h>
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
	async_done ( &aoe->aop, rc );
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
	struct pk_buff *pkb;
	struct aoehdr *aoehdr;
	struct aoecmd *aoecmd;
	unsigned int count;
	unsigned int data_out_len;

	/* Calculate count and data_out_len for this subcommand */
	count = command->cb.count.native;
	if ( count > AOE_MAX_COUNT )
		count = AOE_MAX_COUNT;
	data_out_len = ( command->data_out ? ( count * ATA_SECTOR_SIZE ) : 0 );

	/* Create outgoing packet buffer */
	pkb = alloc_pkb ( ETH_HLEN + sizeof ( *aoehdr ) + sizeof ( *aoecmd ) +
			  data_out_len );
	if ( ! pkb )
		return -ENOMEM;
	pkb->net_protocol = &aoe_protocol;
	pkb_reserve ( pkb, ETH_HLEN );
	aoehdr = pkb_put ( pkb, sizeof ( *aoehdr ) );
	aoecmd = pkb_put ( pkb, sizeof ( *aoecmd ) );
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
	copy_from_user ( pkb_put ( pkb, data_out_len ), command->data_out,
			 aoe->command_offset, data_out_len );

	/* Send packet */
	return net_transmit_via ( pkb, aoe->netdev );
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
	if ( len < ( sizeof ( *aoehdr ) + sizeof ( *aoecmd ) ) )
		return -EINVAL;
	rx_data_len = ( len - sizeof ( *aoehdr ) - sizeof ( *aoecmd ) );

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
 * @v pkb		Packet buffer
 * @ret rc		Return status code
 *
 */
static int aoe_rx ( struct pk_buff *pkb ) {
	struct aoehdr *aoehdr = pkb->data;
	unsigned int len = pkb_len ( pkb );
	struct ethhdr *ethhdr = pkb_push ( pkb, sizeof ( *ethhdr ) );
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
		memcpy ( aoe->target, ethhdr->h_source,
			 sizeof ( aoe->target ) );
		rc = aoe_rx_response ( aoe, aoehdr, len );
		break;
	}

 done:
	free_pkb ( pkb );
	return rc;
}

/**
 * Perform AoE network-layer routing
 *
 * @v pkb	Packet buffer
 * @ret source	Network-layer source address
 * @ret dest	Network-layer destination address
 * @ret rc	Return status code
 */
static int aoe_route ( const struct pk_buff *pkb __unused,
		       struct net_header *nethdr ) {
	struct aoehdr *aoehdr = pkb->data;
	struct aoe_session *aoe;

	list_for_each_entry ( aoe, &aoe_sessions, list ) {
		if ( ( ntohs ( aoehdr->major ) == aoe->major ) &&
		     ( aoehdr->minor == aoe->minor ) ) {
			nethdr->flags = PKT_FL_RAW_ADDR;
			memcpy ( nethdr->dest_net_addr, aoe->target,
				 sizeof ( aoe->target ) );
			return 0;
		}
	}
		
	return -EHOSTUNREACH;
}

/** AoE protocol */
struct net_protocol aoe_protocol = {
	.name = "AoE",
	.net_proto = htons ( ETH_P_AOE ),
	.rx_process = aoe_rx,
	.route = aoe_route,
};

NET_PROTOCOL ( aoe_protocol );

/**
 * Open AoE session
 *
 * @v aoe		AoE session
 */
void aoe_open ( struct aoe_session *aoe ) {
	memset ( aoe->target, 0xff, sizeof ( aoe->target ) );
	list_add ( &aoe->list, &aoe_sessions );
}

/**
 * Close AoE session
 *
 * @v aoe		AoE session
 */
void aoe_close ( struct aoe_session *aoe ) {
	list_del ( &aoe->list );
}

/**
 * Issue ATA command via an open AoE session
 *
 * @v aoe		AoE session
 * @v command		ATA command
 * @ret aop		Asynchronous operation
 *
 * Only one command may be issued concurrently per session.  This call
 * is non-blocking; use async_wait() to wait for the command to
 * complete.
 */
struct async_operation * aoe_issue ( struct aoe_session *aoe,
				     struct ata_command *command ) {
	aoe->command = command;
	aoe->status = 0;
	aoe->command_offset = 0;
	aoe_send_command ( aoe );
	return &aoe->aop;
}
