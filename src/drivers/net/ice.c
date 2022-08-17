/*
 * Copyright (C) 2022 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/iobuf.h>
#include <ipxe/pci.h>
#include "ice.h"

/** @file
 *
 * Intel 100 Gigabit Ethernet network card driver
 *
 */

/**
 * Magic MAC address
 *
 * Used as the source address and promiscuous unicast destination
 * address in the "add switch rules" command.
 */
static uint8_t ice_magic_mac[ETH_HLEN] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };

/******************************************************************************
 *
 * Admin queue
 *
 ******************************************************************************
 */

/**
 * Get firmware version
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int ice_admin_version ( struct intelxl_nic *intelxl ) {
	struct ice_admin_descriptor *cmd;
	struct ice_admin_version_params *version;
	unsigned int api;
	int rc;

	/* Populate descriptor */
	cmd = ice_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_VERSION );
	version = &cmd->params.version;

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
		return rc;
	api = version->api.major;
	DBGC ( intelxl, "ICE %p firmware v%d/%d.%d.%d API v%d/%d.%d.%d\n",
	       intelxl, version->firmware.branch, version->firmware.major,
	       version->firmware.minor, version->firmware.patch,
	       version->api.branch, version->api.major, version->api.minor,
	       version->api.patch );

	/* Check for API compatibility */
	if ( api > INTELXL_ADMIN_API_MAJOR ) {
		DBGC ( intelxl, "ICE %p unsupported API v%d\n", intelxl, api );
		return -ENOTSUP;
	}

	return 0;
}

/**
 * Get MAC address
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int ice_admin_mac_read ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct ice_admin_descriptor *cmd;
	struct ice_admin_mac_read_params *read;
	struct ice_admin_mac_read_address *mac;
	union ice_admin_buffer *buf;
	unsigned int i;
	int rc;

	/* Populate descriptor */
	cmd = ice_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_MAC_READ );
	cmd->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_BUF );
	cmd->len = cpu_to_le16 ( sizeof ( buf->mac_read ) );
	read = &cmd->params.mac_read;
	buf = ice_admin_command_buffer ( intelxl );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
		return rc;

	/* Check that MAC address is present in response */
	if ( ! ( read->valid & INTELXL_ADMIN_MAC_READ_VALID_LAN ) ) {
		DBGC ( intelxl, "ICE %p has no MAC address\n", intelxl );
		return -ENOENT;
	}

	/* Identify MAC address */
	for ( i = 0 ; i < read->count ; i++ ) {

		/* Check for a LAN MAC address */
		mac = &buf->mac_read.mac[i];
		if ( mac->type != ICE_ADMIN_MAC_READ_TYPE_LAN )
			continue;

		/* Check that address is valid */
		if ( ! is_valid_ether_addr ( mac->mac ) ) {
			DBGC ( intelxl, "ICE %p has invalid MAC address "
			       "(%s)\n", intelxl, eth_ntoa ( mac->mac ) );
			return -EINVAL;
		}

		/* Copy MAC address */
		DBGC ( intelxl, "ICE %p has MAC address %s\n",
		       intelxl, eth_ntoa ( mac->mac ) );
		memcpy ( netdev->hw_addr, mac->mac, ETH_ALEN );

		return 0;
	}

	/* Missing LAN MAC address */
	DBGC ( intelxl, "ICE %p has no LAN MAC address\n",
	       intelxl );
	return -ENOENT;
}

/**
 * Set MAC address
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int ice_admin_mac_write ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct ice_admin_descriptor *cmd;
	struct ice_admin_mac_write_params *write;
	int rc;

	/* Populate descriptor */
	cmd = ice_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_MAC_WRITE );
	write = &cmd->params.mac_write;
	memcpy ( write->mac, netdev->ll_addr, ETH_ALEN );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Get switch configuration
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int ice_admin_switch ( struct intelxl_nic *intelxl ) {
	struct ice_admin_descriptor *cmd;
	struct ice_admin_switch_params *sw;
	union ice_admin_buffer *buf;
	uint16_t next = 0;
	uint16_t seid;
	uint16_t type;
	int rc;

	/* Get each configuration in turn */
	do {
		/* Populate descriptor */
		cmd = ice_admin_command_descriptor ( intelxl );
		cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_SWITCH );
		cmd->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_BUF );
		cmd->len = cpu_to_le16 ( sizeof ( buf->sw ) );
		sw = &cmd->params.sw;
		sw->next = next;
		buf = ice_admin_command_buffer ( intelxl );

		/* Issue command */
		if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
			return rc;
		seid = le16_to_cpu ( buf->sw.cfg[0].seid );

		/* Dump raw configuration */
		DBGC2 ( intelxl, "ICE %p SEID %#04x:\n", intelxl, seid );
		DBGC2_HDA ( intelxl, 0, &buf->sw.cfg[0],
			    sizeof ( buf->sw.cfg[0] ) );

		/* Parse response */
		type = ( seid & ICE_ADMIN_SWITCH_TYPE_MASK );
		if ( type == ICE_ADMIN_SWITCH_TYPE_VSI ) {
			intelxl->vsi = ( seid & ~ICE_ADMIN_SWITCH_TYPE_MASK );
			DBGC ( intelxl, "ICE %p VSI %#04x uplink %#04x func "
			       "%#04x\n", intelxl, intelxl->vsi,
			       le16_to_cpu ( buf->sw.cfg[0].uplink ),
			       le16_to_cpu ( buf->sw.cfg[0].func ) );
		}

	} while ( ( next = sw->next ) );

	/* Check that we found a VSI */
	if ( ! intelxl->vsi ) {
		DBGC ( intelxl, "ICE %p has no VSI\n", intelxl );
		return -ENOENT;
	}

	return 0;
}

/**
 * Add switch rules
 *
 * @v intelxl		Intel device
 * @v mac		MAC address
 * @ret rc		Return status code
 */
static int ice_admin_rules ( struct intelxl_nic *intelxl, uint8_t *mac ) {
	struct ice_admin_descriptor *cmd;
	struct ice_admin_rules_params *rules;
	union ice_admin_buffer *buf;
	int rc;

	/* Populate descriptor */
	cmd = ice_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( ICE_ADMIN_ADD_RULES );
	cmd->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_BUF | INTELXL_ADMIN_FL_RD );
	cmd->len = cpu_to_le16 ( sizeof ( buf->rules ) );
	rules = &cmd->params.rules;
	rules->count = cpu_to_le16 ( 1 );
	buf = ice_admin_command_buffer ( intelxl );
	buf->rules.recipe = cpu_to_le16 ( ICE_ADMIN_RULES_RECIPE_PROMISC );
	buf->rules.port = cpu_to_le16 ( intelxl->port );
	buf->rules.action =
		cpu_to_le32 ( ICE_ADMIN_RULES_ACTION_VALID |
			      ICE_ADMIN_RULES_ACTION_VSI ( intelxl->vsi ) );
	buf->rules.len = cpu_to_le16 ( sizeof ( buf->rules.hdr ) );
	memcpy ( buf->rules.hdr.eth.h_dest, mac, ETH_ALEN );
	memcpy ( buf->rules.hdr.eth.h_source, ice_magic_mac, ETH_ALEN );
	buf->rules.hdr.eth.h_protocol = htons ( ETH_P_8021Q );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Check if scheduler node is a parent (i.e. non-leaf) node
 *
 * @v branch		Scheduler topology branch
 * @v node		Scheduler topology node
 * @ret child		Any child node, or NULL if not found
 */
static struct ice_admin_schedule_node *
ice_admin_schedule_is_parent ( struct ice_admin_schedule_branch *branch,
			       struct ice_admin_schedule_node *node ) {
	unsigned int count = le16_to_cpu ( branch->count );
	struct ice_admin_schedule_node *child;
	unsigned int i;

	/* Find a child element, if any */
	for ( i = 0 ; i < count ; i++ ) {
		child = &branch->node[i];
		if ( child->parent == node->teid )
			return child;
	}

	return NULL;
}

/**
 * Query default scheduling tree topology
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int ice_admin_schedule ( struct intelxl_nic *intelxl ) {
	struct ice_admin_descriptor *cmd;
	struct ice_admin_schedule_params *sched;
	struct ice_admin_schedule_branch *branch;
	struct ice_admin_schedule_node *node;
	union ice_admin_buffer *buf;
	int i;
	int rc;

	/* Populate descriptor */
	cmd = ice_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( ICE_ADMIN_SCHEDULE );
	cmd->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_BUF );
	cmd->len = cpu_to_le16 ( sizeof ( buf->sched ) );
	sched = &cmd->params.sched;
	buf = ice_admin_command_buffer ( intelxl );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
		return rc;

	/* Sanity checks */
	if ( ! sched->branches ) {
		DBGC ( intelxl, "ICE %p topology has no branches\n", intelxl );
		return -EINVAL;
	}
	branch = buf->sched.branch;

	/* Identify leaf node */
	for ( i = ( le16_to_cpu ( branch->count ) - 1 ) ; i >= 0 ; i-- ) {
		node = &branch->node[i];
		if ( ! ice_admin_schedule_is_parent ( branch, node ) ) {
			intelxl->teid = le32_to_cpu ( node->teid );
			DBGC2 ( intelxl, "ICE %p TEID %#08x type %d\n",
				intelxl, intelxl->teid, node->config.type );
			break;
		}
	}
	if ( ! intelxl->teid ) {
		DBGC ( intelxl, "ICE %p found no leaf TEID\n", intelxl );
		return -EINVAL;
	}

	return 0;
}

/**
 * Restart autonegotiation
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int ice_admin_autoneg ( struct intelxl_nic *intelxl ) {
	struct ice_admin_descriptor *cmd;
	struct ice_admin_autoneg_params *autoneg;
	int rc;

	/* Populate descriptor */
	cmd = ice_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_AUTONEG );
	autoneg = &cmd->params.autoneg;
	autoneg->flags = ( INTELXL_ADMIN_AUTONEG_FL_RESTART |
			   INTELXL_ADMIN_AUTONEG_FL_ENABLE );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Get link status
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int ice_admin_link ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct ice_admin_descriptor *cmd;
	struct ice_admin_link_params *link;
	union ice_admin_buffer *buf;
	int rc;

	/* Populate descriptor */
	cmd = ice_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_LINK );
	cmd->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_BUF );
	cmd->len = cpu_to_le16 ( sizeof ( buf->link ) );
	link = &cmd->params.link;
	link->notify = INTELXL_ADMIN_LINK_NOTIFY;
	buf = ice_admin_command_buffer ( intelxl );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
		return rc;
	DBGC ( intelxl, "ICE %p speed %#02x status %#02x\n",
	       intelxl, le16_to_cpu ( buf->link.speed ), buf->link.status );

	/* Update network device */
	if ( buf->link.status & INTELXL_ADMIN_LINK_UP ) {
		netdev_link_up ( netdev );
	} else {
		netdev_link_down ( netdev );
	}

	return 0;
}

/**
 * Handle admin event
 *
 * @v netdev		Network device
 * @v xlevt		Event descriptor
 * @v xlbuf		Data buffer
 */
static void ice_admin_event ( struct net_device *netdev,
			      struct intelxl_admin_descriptor *xlevt,
			      union intelxl_admin_buffer *xlbuf __unused ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct ice_admin_descriptor *evt =
		container_of ( xlevt, struct ice_admin_descriptor, xl );

	/* Ignore unrecognised events */
	if ( evt->opcode != cpu_to_le16 ( INTELXL_ADMIN_LINK ) ) {
		DBGC ( intelxl, "INTELXL %p unrecognised event opcode "
		       "%#04x\n", intelxl, le16_to_cpu ( evt->opcode ) );
		return;
	}

	/* Update link status */
	ice_admin_link ( netdev );
}

/**
 * Add transmit queue
 *
 * @v intelxl		Intel device
 * @v ring		Descriptor ring
 * @ret rc		Return status code
 */
static int ice_admin_add_txq ( struct intelxl_nic *intelxl,
			       struct intelxl_ring *ring ) {
	struct ice_admin_descriptor *cmd;
	struct ice_admin_add_txq_params *add_txq;
	union ice_admin_buffer *buf;
	struct ice_context_tx *ctx;
	struct ice_schedule_tx *sched;
	physaddr_t address;
	int rc;

	/* Populate descriptor */
	cmd = ice_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( ICE_ADMIN_ADD_TXQ );
	cmd->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_RD | INTELXL_ADMIN_FL_BUF );
	cmd->len = cpu_to_le16 ( sizeof ( buf->add_txq ) );
	add_txq = &cmd->params.add_txq;
	add_txq->count = 1;
	buf = ice_admin_command_buffer ( intelxl );
	buf->add_txq.parent = cpu_to_le32 ( intelxl->teid );
	buf->add_txq.count = 1;
	ctx = &buf->add_txq.ctx;
	address = dma ( &ring->map, ring->desc.raw );
	ctx->base_port =
		cpu_to_le64 ( ICE_TXQ_BASE_PORT ( address, intelxl->port ) );
	ctx->pf_type = cpu_to_le16 ( ICE_TXQ_PF_TYPE ( intelxl->pf ) );
	ctx->vsi = cpu_to_le16 ( intelxl->vsi );
	ctx->len = cpu_to_le16 ( ICE_TXQ_LEN ( INTELXL_TX_NUM_DESC ) );
	ctx->flags = cpu_to_le16 ( ICE_TXQ_FL_TSO | ICE_TXQ_FL_LEGACY );
	sched = &buf->add_txq.sched;
	sched->sections = ( ICE_SCHEDULE_GENERIC | ICE_SCHEDULE_COMMIT |
			    ICE_SCHEDULE_EXCESS );
	sched->commit_weight = cpu_to_le16 ( ICE_SCHEDULE_WEIGHT );
	sched->excess_weight = cpu_to_le16 ( ICE_SCHEDULE_WEIGHT );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
		return rc;
	DBGC ( intelxl, "ICE %p added TEID %#04x\n",
	       intelxl, le32_to_cpu ( buf->add_txq.teid ) );

	return 0;
}

/**
 * Disable transmit queue
 *
 * @v intelxl		Intel device
 * @v ring		Descriptor ring
 * @ret rc		Return status code
 */
static int ice_admin_disable_txq ( struct intelxl_nic *intelxl ) {
	struct ice_admin_descriptor *cmd;
	struct ice_admin_disable_txq_params *disable_txq;
	union ice_admin_buffer *buf;
	int rc;

	/* Populate descriptor */
	cmd = ice_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( ICE_ADMIN_DISABLE_TXQ );
	cmd->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_RD | INTELXL_ADMIN_FL_BUF );
	cmd->len = cpu_to_le16 ( sizeof ( buf->disable_txq ) );
	disable_txq = &cmd->params.disable_txq;
	disable_txq->flags = ICE_TXQ_FL_FLUSH;
	disable_txq->count = 1;
	disable_txq->timeout = ICE_TXQ_TIMEOUT;
	buf = ice_admin_command_buffer ( intelxl );
	buf->disable_txq.parent = cpu_to_le32 ( intelxl->teid );
	buf->disable_txq.count = 1;

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
		return rc;

	return 0;
}

/******************************************************************************
 *
 * Network device interface
 *
 ******************************************************************************
 */

/**
 * Dump transmit queue context (for debugging)
 *
 * @v intelxl		Intel device
 */
static void ice_dump_tx ( struct intelxl_nic *intelxl ) {
	uint32_t ctx[ sizeof ( struct ice_context_tx ) / sizeof ( uint32_t ) ];
	uint32_t stat;
	unsigned int i;

	/* Do nothing unless debug output is enabled */
	if ( ! DBG_EXTRA )
		return;

	/* Trigger reading of transmit context */
	writel ( ( ICE_GLCOMM_QTX_CNTX_CTL_CMD_READ |
		   ICE_GLCOMM_QTX_CNTX_CTL_EXEC ),
		 intelxl->regs + ICE_GLCOMM_QTX_CNTX_CTL );

	/* Wait for operation to complete */
	for ( i = 0 ; i < INTELXL_CTX_MAX_WAIT_MS ; i++ ) {

		/* Check if operation is complete */
		stat = readl ( intelxl->regs + ICE_GLCOMM_QTX_CNTX_STAT );
		if ( ! ( stat & ICE_GLCOMM_QTX_CNTX_BUSY ) )
			break;

		/* Delay */
		mdelay ( 1 );
	}

	/* Read context registers */
	for ( i = 0 ; i < ( sizeof ( ctx ) / sizeof ( ctx[0] ) ) ; i++ ) {
		ctx[i] = cpu_to_le32 ( readl ( intelxl->regs +
					       ICE_GLCOMM_QTX_CNTX_DATA ( i )));
	}

	/* Dump context */
	DBGC2 ( intelxl, "ICE %p TX context:\n", intelxl );
	DBGC2_HDA ( intelxl, 0, ctx, sizeof ( ctx ) );
}

/**
 * Dump receive queue context (for debugging)
 *
 * @v intelxl		Intel device
 */
static void ice_dump_rx ( struct intelxl_nic *intelxl ) {
	uint32_t ctx[ sizeof ( struct intelxl_context_rx ) /
		      sizeof ( uint32_t ) ];
	unsigned int i;

	/* Do nothing unless debug output is enabled */
	if ( ! DBG_EXTRA )
		return;

	/* Read context registers */
	for ( i = 0 ; i < ( sizeof ( ctx ) / sizeof ( ctx[0] ) ) ; i++ ) {
		ctx[i] = cpu_to_le32 ( readl ( intelxl->regs +
					       ICE_QRX_CONTEXT ( i ) ) );
	}

	/* Dump context */
	DBGC2 ( intelxl, "ICE %p RX context:\n", intelxl );
	DBGC2_HDA ( intelxl, 0, ctx, sizeof ( ctx ) );
}

/**
 * Create transmit queue
 *
 * @v intelxl		Intel device
 * @v ring		Descriptor ring
 * @ret rc		Return status code
 */
static int ice_create_tx ( struct intelxl_nic *intelxl,
			   struct intelxl_ring *ring ) {
	int rc;

	/* Allocate descriptor ring */
	if ( ( rc = intelxl_alloc_ring ( intelxl, ring ) ) != 0 )
		goto err_alloc;

	/* Add transmit queue */
	if ( ( rc = ice_admin_add_txq ( intelxl, ring ) ) != 0 )
		goto err_add_txq;

	return 0;

 err_add_txq:
	intelxl_free_ring ( intelxl, ring );
 err_alloc:
	return rc;
}

/**
 * Destroy transmit queue
 *
 * @v intelxl		Intel device
 * @v ring		Descriptor ring
 * @ret rc		Return status code
 */
static void ice_destroy_tx ( struct intelxl_nic *intelxl,
			     struct intelxl_ring *ring ) {
	int rc;

	/* Disable transmit queue */
	if ( ( rc = ice_admin_disable_txq ( intelxl ) ) != 0 ) {
		/* Leak memory; there's nothing else we can do */
		return;
	}

	/* Free descriptor ring */
	intelxl_free_ring ( intelxl, ring );
}

/**
 * Program receive queue context
 *
 * @v intelxl		Intel device
 * @v address		Descriptor ring base address
 * @ret rc		Return status code
 */
static int ice_context_rx ( struct intelxl_nic *intelxl,
			    physaddr_t address ) {
	union {
		struct intelxl_context_rx rx;
		uint32_t raw[ sizeof ( struct intelxl_context_rx ) /
			      sizeof ( uint32_t ) ];
	} ctx;
	uint64_t base_count;
	unsigned int i;

	/* Initialise context */
	memset ( &ctx, 0, sizeof ( ctx ) );
	base_count = INTELXL_CTX_RX_BASE_COUNT ( address, INTELXL_RX_NUM_DESC );
	ctx.rx.base_count = cpu_to_le64 ( base_count );
	ctx.rx.len = cpu_to_le16 ( INTELXL_CTX_RX_LEN ( intelxl->mfs ) );
	ctx.rx.flags = ( INTELXL_CTX_RX_FL_DSIZE | INTELXL_CTX_RX_FL_CRCSTRIP );
	ctx.rx.mfs = cpu_to_le16 ( INTELXL_CTX_RX_MFS ( intelxl->mfs ) );

	/* Write context registers */
	for ( i = 0 ; i < ( sizeof ( ctx ) / sizeof ( ctx.raw[0] ) ) ; i++ ) {
		writel ( le32_to_cpu ( ctx.raw[i] ),
			 ( intelxl->regs + ICE_QRX_CONTEXT ( i ) ) );
	}

	return 0;
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int ice_open ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	int rc;

	/* Calculate maximum frame size */
	intelxl->mfs = ( ( ETH_HLEN + netdev->mtu + 4 /* CRC */ +
			   INTELXL_ALIGN - 1 ) & ~( INTELXL_ALIGN - 1 ) );

	/* Set MAC address */
	if ( ( rc = ice_admin_mac_write ( netdev ) ) != 0 )
		goto err_mac_write;

	/* Set maximum frame size */
	if ( ( rc = intelxl_admin_mac_config ( intelxl ) ) != 0 )
		goto err_mac_config;

	/* Create receive descriptor ring */
	if ( ( rc = intelxl_create_ring ( intelxl, &intelxl->rx ) ) != 0 )
		goto err_create_rx;

	/* Create transmit descriptor ring */
	if ( ( rc = ice_create_tx ( intelxl, &intelxl->tx ) ) != 0 )
		goto err_create_tx;

	/* Restart autonegotiation */
	ice_admin_autoneg ( intelxl );

	/* Update link state */
	ice_admin_link ( netdev );

	return 0;

	ice_destroy_tx ( intelxl, &intelxl->tx );
 err_create_tx:
	intelxl_destroy_ring ( intelxl, &intelxl->rx );
 err_create_rx:
 err_mac_config:
 err_mac_write:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void ice_close ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;

	/* Dump contexts (for debugging) */
	ice_dump_tx ( intelxl );
	ice_dump_rx ( intelxl );

	/* Destroy transmit descriptor ring */
	ice_destroy_tx ( intelxl, &intelxl->tx );

	/* Destroy receive descriptor ring */
	intelxl_destroy_ring ( intelxl, &intelxl->rx );

	/* Discard any unused receive buffers */
	intelxl_empty_rx ( intelxl );
}

/** Network device operations */
static struct net_device_operations ice_operations = {
	.open		= ice_open,
	.close		= ice_close,
	.transmit	= intelxl_transmit,
	.poll		= intelxl_poll,
};

/******************************************************************************
 *
 * PCI interface
 *
 ******************************************************************************
 */

/**
 * Probe PCI device
 *
 * @v pci		PCI device
 * @ret rc		Return status code
 */
static int ice_probe ( struct pci_device *pci ) {
	struct net_device *netdev;
	struct intelxl_nic *intelxl;
	uint32_t pffunc_rid;
	uint32_t pfgen_portnum;
	int rc;

	/* Allocate and initialise net device */
	netdev = alloc_etherdev ( sizeof ( *intelxl ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &ice_operations );
	netdev->max_pkt_len = INTELXL_MAX_PKT_LEN;
	intelxl = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	netdev->dev = &pci->dev;
	memset ( intelxl, 0, sizeof ( *intelxl ) );
	intelxl->intr = ICE_GLINT_DYN_CTL;
	intelxl->handle = ice_admin_event;
	intelxl_init_admin ( &intelxl->command, INTELXL_ADMIN_CMD,
			     &intelxl_admin_offsets );
	intelxl_init_admin ( &intelxl->event, INTELXL_ADMIN_EVT,
			     &intelxl_admin_offsets );
	intelxl_init_ring ( &intelxl->tx, INTELXL_TX_NUM_DESC,
			    sizeof ( intelxl->tx.desc.tx[0] ), NULL );
	intelxl_init_ring ( &intelxl->rx, INTELXL_RX_NUM_DESC,
			    sizeof ( intelxl->rx.desc.rx[0] ),
			    ice_context_rx );

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Map registers */
	intelxl->regs = pci_ioremap ( pci, pci->membase, ICE_BAR_SIZE );
	if ( ! intelxl->regs ) {
		rc = -ENODEV;
		goto err_ioremap;
	}

	/* Configure DMA */
	intelxl->dma = &pci->dma;
	dma_set_mask_64bit ( intelxl->dma );
	netdev->dma = intelxl->dma;

	/* Locate PCI Express capability */
	intelxl->exp = pci_find_capability ( pci, PCI_CAP_ID_EXP );
	if ( ! intelxl->exp ) {
		DBGC ( intelxl, "ICE %p missing PCIe capability\n",
		       intelxl );
		rc = -ENXIO;
		goto err_exp;
	}

	/* Reset the function via PCIe FLR */
	pci_reset ( pci, intelxl->exp );

	/* Get function and port number */
	pffunc_rid = readl ( intelxl->regs + ICE_PFFUNC_RID );
	intelxl->pf = ICE_PFFUNC_RID_FUNC_NUM ( pffunc_rid );
	pfgen_portnum = readl ( intelxl->regs + ICE_PFGEN_PORTNUM );
	intelxl->port = ICE_PFGEN_PORTNUM_PORT_NUM ( pfgen_portnum );
	DBGC ( intelxl, "ICE %p PF %d using port %d\n",
	       intelxl, intelxl->pf, intelxl->port );

	/* Enable MSI-X dummy interrupt */
	if ( ( rc = intelxl_msix_enable ( intelxl, pci,
					  INTELXL_MSIX_VECTOR ) ) != 0 )
		goto err_msix;

	/* Open admin queues */
	if ( ( rc = intelxl_open_admin ( intelxl ) ) != 0 )
		goto err_open_admin;

	/* Get firmware version */
	if ( ( rc = ice_admin_version ( intelxl ) ) != 0 )
		goto err_admin_version;

	/* Clear PXE mode */
	if ( ( rc = intelxl_admin_clear_pxe ( intelxl ) ) != 0 )
		goto err_admin_clear_pxe;

	/* Get switch configuration */
	if ( ( rc = ice_admin_switch ( intelxl ) ) != 0 )
		goto err_admin_switch;

	/* Add broadcast address */
	if ( ( rc = ice_admin_rules ( intelxl, eth_broadcast ) ) != 0 )
		goto err_admin_rules_broadcast;

	/* Add promiscuous unicast address */
	if ( ( rc = ice_admin_rules ( intelxl, ice_magic_mac ) ) != 0 )
		goto err_admin_rules_magic;

	/* Query scheduler topology */
	if ( ( rc = ice_admin_schedule ( intelxl ) ) != 0 )
		goto err_admin_schedule;

	/* Get MAC address */
	if ( ( rc = ice_admin_mac_read ( netdev ) ) != 0 )
		goto err_admin_mac_read;

	/* Configure queue register addresses */
	intelxl->tx.tail = ICE_QTX_COMM_DBELL;
	intelxl->rx.reg = ICE_QRX_CTRL;
	intelxl->rx.tail = ICE_QRX_TAIL;

	/* Configure interrupt causes */
	writel ( ( ICE_QINT_TQCTL_ITR_INDX_NONE | ICE_QINT_TQCTL_CAUSE_ENA ),
		 intelxl->regs + ICE_QINT_TQCTL );
	writel ( ( ICE_QINT_RQCTL_ITR_INDX_NONE | ICE_QINT_RQCTL_CAUSE_ENA ),
		 intelxl->regs + ICE_QINT_RQCTL );

	/* Set a default value for the queue context flex extension,
	 * since this register erroneously retains its value across at
	 * least a PCIe FLR.
	 */
	writel ( ( ICE_QRX_FLXP_CNTXT_RXDID_IDX_LEGACY_32 |
		   ICE_QRX_FLXP_CNTXT_RXDID_PRIO_MAX ),
		 intelxl->regs + ICE_QRX_FLXP_CNTXT );

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	/* Set initial link state */
	ice_admin_link ( netdev );

	return 0;

	unregister_netdev ( netdev );
 err_register_netdev:
 err_admin_mac_read:
 err_admin_schedule:
 err_admin_rules_magic:
 err_admin_rules_broadcast:
 err_admin_switch:
 err_admin_clear_pxe:
 err_admin_version:
	intelxl_close_admin ( intelxl );
 err_open_admin:
	intelxl_msix_disable ( intelxl, pci, INTELXL_MSIX_VECTOR );
 err_msix:
	pci_reset ( pci, intelxl->exp );
 err_exp:
	iounmap ( intelxl->regs );
 err_ioremap:
	netdev_nullify ( netdev );
	netdev_put ( netdev );
 err_alloc:
	return rc;
}

/**
 * Remove PCI device
 *
 * @v pci		PCI device
 */
static void ice_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct intelxl_nic *intelxl = netdev->priv;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Close admin queues */
	intelxl_close_admin ( intelxl );

	/* Disable MSI-X dummy interrupt */
	intelxl_msix_disable ( intelxl, pci, INTELXL_MSIX_VECTOR );

	/* Reset the NIC */
	pci_reset ( pci, intelxl->exp );

	/* Free network device */
	iounmap ( intelxl->regs );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** PCI device IDs */
static struct pci_device_id ice_nics[] = {
	PCI_ROM ( 0x8086, 0x124c, "e823l-bp", "E823-L backplane", 0 ),
	PCI_ROM ( 0x8086, 0x124d, "e823l-sfp", "E823-L SFP", 0 ),
	PCI_ROM ( 0x8086, 0x124e, "e823l-10gt", "E823-L 10GBASE-T", 0 ),
	PCI_ROM ( 0x8086, 0x124f, "e823l-1g", "E823-L 1GbE", 0 ),
	PCI_ROM ( 0x8086, 0x151d, "e823l-qsfp", "E823-L QSFP", 0 ),
	PCI_ROM ( 0x8086, 0x1591, "e810c-bp", "E810-C backplane", 0 ),
	PCI_ROM ( 0x8086, 0x1592, "e810c-qsfp", "E810-C QSFP", 0 ),
	PCI_ROM ( 0x8086, 0x1593, "e810c-sfp", "E810-C SFP", 0 ),
	PCI_ROM ( 0x8086, 0x1599, "e810-xxv-bp", "E810-XXV backplane", 0 ),
	PCI_ROM ( 0x8086, 0x159a, "e810-xxv-qsfp", "E810-XXV QSFP", 0 ),
	PCI_ROM ( 0x8086, 0x159b, "e810-xxv-sfp", "E810-XXV SFP", 0 ),
	PCI_ROM ( 0x8086, 0x188a, "e823c-bp", "E823-C backplane", 0 ),
	PCI_ROM ( 0x8086, 0x188b, "e823c-qsfp", "E823-C QSFP", 0 ),
	PCI_ROM ( 0x8086, 0x188c, "e823c-sfp", "E823-C SFP", 0 ),
	PCI_ROM ( 0x8086, 0x188d, "e823c-10gt", "E823-C 10GBASE-T", 0 ),
	PCI_ROM ( 0x8086, 0x188e, "e823c-1g", "E823-C 1GbE", 0 ),
	PCI_ROM ( 0x8086, 0x1890, "e822c-bp", "E822-C backplane", 0 ),
	PCI_ROM ( 0x8086, 0x1891, "e822c-qsfp", "E822-C QSFP", 0 ),
	PCI_ROM ( 0x8086, 0x1892, "e822c-sfp", "E822-C SFP", 0 ),
	PCI_ROM ( 0x8086, 0x1893, "e822c-10gt", "E822-C 10GBASE-T", 0 ),
	PCI_ROM ( 0x8086, 0x1894, "e822c-1g", "E822-C 1GbE", 0 ),
	PCI_ROM ( 0x8086, 0x1897, "e822l-bp", "E822-L backplane", 0 ),
	PCI_ROM ( 0x8086, 0x1898, "e822l-sfp", "E822-L SFP", 0 ),
	PCI_ROM ( 0x8086, 0x1899, "e822l-10gt", "E822-L 10GBASE-T", 0 ),
	PCI_ROM ( 0x8086, 0x189a, "e822l-1g", "E822-L 1GbE", 0 ),
};

/** PCI driver */
struct pci_driver ice_driver __pci_driver = {
	.ids = ice_nics,
	.id_count = ( sizeof ( ice_nics ) / sizeof ( ice_nics[0] ) ),
	.probe = ice_probe,
	.remove = ice_remove,
};
