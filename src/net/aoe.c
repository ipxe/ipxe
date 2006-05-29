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
#include <gpxe/process.h>
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

	/* Create outgoing packet buffer */
	pkb = alloc_pkb ( ETH_HLEN + sizeof ( *aoehdr ) + sizeof ( *aoecmd ) +
			  command->data_out_len );
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
			   ( command->data_out_len ? AOE_FL_WRITE : 0 ) );
	aoecmd->err_feat = command->cb.err_feat.bytes.cur;
	aoecmd->count = command->cb.count.bytes.cur;
	aoecmd->cmd_stat = command->cb.cmd_stat;
	aoecmd->lba.u64 = cpu_to_le64 ( command->cb.lba.native );
	if ( ! command->cb.lba48 )
		aoecmd->lba.bytes[3] |= ( command->cb.device & ATA_DEV_MASK );

	/* Fill data payload */
	copy_from_user ( pkb_put ( pkb, command->data_out_len ),
			 command->data_out, aoe->command_offset,
			 command->data_out_len );

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
	unsigned int data_in_len;
	
	/* Sanity check */
	if ( len < ( sizeof ( *aoehdr ) + sizeof ( *aoecmd ) ) )
		return -EINVAL;

	/* Set overall status code */
	aoe->status = ( ( aoehdr->ver_flags & AOE_FL_ERROR ) ?
			aoehdr->error : 0 );

	/* Copy ATA results */
	command->cb.err_feat.bytes.cur = aoecmd->err_feat;
	command->cb.count.bytes.cur = aoecmd->count;
	command->cb.cmd_stat = aoecmd->cmd_stat;
	command->cb.lba.native = le64_to_cpu ( aoecmd->lba.u64 );
	command->cb.lba.bytes.pad = 0;
	
	/* Copy data payload */
	data_in_len = ( len - sizeof ( *aoehdr ) - sizeof ( *aoecmd ) );
	if ( data_in_len > command->data_in_len ) {
		data_in_len = command->data_in_len;
		aoe->status |= AOE_STATUS_OVERRUN;
	} else if ( data_in_len < command->data_in_len ) {
		aoe->status |= AOE_STATUS_UNDERRUN;
	}
	copy_to_user ( command->data_in, aoe->command_offset,
		       aoecmd->data, data_in_len );

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

#warning "Need a way to get the MAC address for future reference"

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

#warning "Need a way to find out the MAC address"
	nethdr->flags = PKT_FL_BROADCAST;
	return 0;
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
 * Kick an AoE session into life
 *
 * @v aoe		AoE session
 *
 * Transmits an AoE request.  Call this function to issue a new
 * command, or when a retransmission timer expires.
 */
void aoe_kick ( struct aoe_session *aoe ) {
	aoe_send_command ( aoe );
}

/**
 * Issue ATA command via an open AoE session
 *
 * @v aoe		AoE session
 * @v command		ATA command
 * @ret rc		Return status code
 *
 * The ATA command must fit within a single AoE frame (i.e. the sector
 * count must not exceed AOE_MAX_COUNT).
 */
int aoe_issue ( struct aoe_session *aoe, struct ata_command *command ) {
	aoe->command = command;
	aoe->status = AOE_STATUS_PENDING;

	aoe_kick ( aoe );
	while ( aoe->status & AOE_STATUS_PENDING ) {
		step();
	}
	aoe->command = NULL;

	return ( ( aoe->status & AOE_STATUS_ERR_MASK ) ? -EIO : 0 );
}

/**
 * Issue ATA command via an open AoE session
 *
 * @v aoe		AoE session
 * @v command		ATA command
 * @ret rc		Return status code
 *
 * The ATA command will be split into several smaller ATA commands,
 * each with a sector count no larger than AOE_MAX_COUNT.
 */
int aoe_issue_split ( struct aoe_session *aoe, struct ata_command *command ) {
	struct ata_command subcommand;
	unsigned int offset;
	unsigned int count;
	unsigned int data_len;
	unsigned int status = 0;
	int rc = 0;

	/* Split ATA command into AoE-sized subcommands */
	for ( offset = 0; offset < command->cb.count.native; offset += count ){
		memcpy ( &subcommand, command, sizeof ( subcommand ) );
		count = ( command->cb.count.native - offset );
		if ( count > AOE_MAX_COUNT )
			count = AOE_MAX_COUNT;
		data_len = count * ATA_SECTOR_SIZE;
		if ( subcommand.data_in_len )
			subcommand.data_in_len = data_len;
		if ( subcommand.data_out_len )
			subcommand.data_out_len = data_len;
		aoe->command_offset = ( offset * ATA_SECTOR_SIZE );
		subcommand.cb.lba.native += offset;
		subcommand.cb.count.native = count;
		if ( ( rc = aoe_issue ( aoe, &subcommand ) ) != 0 )
			goto done;
		status |= subcommand.cb.cmd_stat;
	}
	command->cb.cmd_stat = status;

 done:
	aoe->command_offset = 0;
	return rc;
}
