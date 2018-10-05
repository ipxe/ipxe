/*
 * Copyright (C) 2018 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/iobuf.h>
#include <ipxe/malloc.h>
#include <ipxe/pci.h>
#include <ipxe/version.h>
#include "intelxl.h"

/** @file
 *
 * Intel 40 Gigabit Ethernet network card driver
 *
 */

/******************************************************************************
 *
 * Device reset
 *
 ******************************************************************************
 */

/**
 * Reset hardware
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int intelxl_reset ( struct intelxl_nic *intelxl ) {
	uint32_t pfgen_ctrl;

	/* Perform a global software reset */
	pfgen_ctrl = readl ( intelxl->regs + INTELXL_PFGEN_CTRL );
	writel ( ( pfgen_ctrl | INTELXL_PFGEN_CTRL_PFSWR ),
		 intelxl->regs + INTELXL_PFGEN_CTRL );
	mdelay ( INTELXL_RESET_DELAY_MS );

	return 0;
}

/******************************************************************************
 *
 * MAC address
 *
 ******************************************************************************
 */

/**
 * Fetch initial MAC address and maximum frame size
 *
 * @v intelxl		Intel device
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int intelxl_fetch_mac ( struct intelxl_nic *intelxl,
			       struct net_device *netdev ) {
	union intelxl_receive_address mac;
	uint32_t prtgl_sal;
	uint32_t prtgl_sah;
	size_t mfs;

	/* Read NVM-loaded address */
	prtgl_sal = readl ( intelxl->regs + INTELXL_PRTGL_SAL );
	prtgl_sah = readl ( intelxl->regs + INTELXL_PRTGL_SAH );
	mac.reg.low = cpu_to_le32 ( prtgl_sal );
	mac.reg.high = cpu_to_le32 ( prtgl_sah );

	/* Check that address is valid */
	if ( ! is_valid_ether_addr ( mac.raw ) ) {
		DBGC ( intelxl, "INTELXL %p has invalid MAC address (%s)\n",
		       intelxl, eth_ntoa ( mac.raw ) );
		return -ENOENT;
	}

	/* Copy MAC address */
	DBGC ( intelxl, "INTELXL %p has autoloaded MAC address %s\n",
	       intelxl, eth_ntoa ( mac.raw ) );
	memcpy ( netdev->hw_addr, mac.raw, ETH_ALEN );

	/* Get maximum frame size */
	mfs = INTELXL_PRTGL_SAH_MFS_GET ( prtgl_sah );
	netdev->max_pkt_len = ( mfs - 4 /* CRC */ );

	return 0;
}

/******************************************************************************
 *
 * Admin queue
 *
 ******************************************************************************
 */

/**
 * Create admin queue
 *
 * @v intelxl		Intel device
 * @v admin		Admin queue
 * @ret rc		Return status code
 */
static int intelxl_create_admin ( struct intelxl_nic *intelxl,
				  struct intelxl_admin *admin ) {
	size_t len = ( sizeof ( admin->desc[0] ) * INTELXL_ADMIN_NUM_DESC );
	void *admin_regs = ( intelxl->regs + admin->reg );
	physaddr_t address;

	/* Allocate admin queue */
	admin->desc = malloc_dma ( ( len + sizeof ( *admin->buffer ) ),
				   INTELXL_ALIGN );
	if ( ! admin->desc )
		return -ENOMEM;
	admin->buffer = ( ( ( void * ) admin->desc ) + len );

	/* Initialise admin queue */
	memset ( admin->desc, 0, len );

	/* Reset head and tail registers */
	writel ( 0, admin_regs + INTELXL_ADMIN_HEAD );
	writel ( 0, admin_regs + INTELXL_ADMIN_TAIL );

	/* Reset queue index */
	admin->index = 0;

	/* Program queue address */
	address = virt_to_bus ( admin->desc );
	writel ( ( address & 0xffffffffUL ), admin_regs + INTELXL_ADMIN_BAL );
	if ( sizeof ( physaddr_t ) > sizeof ( uint32_t ) ) {
		writel ( ( ( ( uint64_t ) address ) >> 32 ),
			 admin_regs + INTELXL_ADMIN_BAH );
	} else {
		writel ( 0, admin_regs + INTELXL_ADMIN_BAH );
	}

	/* Program queue length and enable queue */
	writel ( ( INTELXL_ADMIN_LEN_LEN ( INTELXL_ADMIN_NUM_DESC ) |
		   INTELXL_ADMIN_LEN_ENABLE ),
		 admin_regs + INTELXL_ADMIN_LEN );

	DBGC ( intelxl, "INTELXL %p A%cQ is at [%08llx,%08llx) buf "
	       "[%08llx,%08llx)\n", intelxl,
	       ( ( admin->reg == INTELXL_ADMIN_CMD ) ? 'T' : 'R' ),
	       ( ( unsigned long long ) address ),
	       ( ( unsigned long long ) address + len ),
	       ( ( unsigned long long ) virt_to_bus ( admin->buffer ) ),
	       ( ( unsigned long long ) ( virt_to_bus ( admin->buffer ) +
					  sizeof ( admin->buffer[0] ) ) ) );
	return 0;
}

/**
 * Destroy admin queue
 *
 * @v intelxl		Intel device
 * @v admin		Admin queue
 */
static void intelxl_destroy_admin ( struct intelxl_nic *intelxl,
				    struct intelxl_admin *admin ) {
	size_t len = ( sizeof ( admin->desc[0] ) * INTELXL_ADMIN_NUM_DESC );
	void *admin_regs = ( intelxl->regs + admin->reg );

	/* Disable queue */
	writel ( 0, admin_regs + INTELXL_ADMIN_LEN );

	/* Free queue */
	free_dma ( admin->desc, ( len + sizeof ( *admin->buffer ) ) );
}

/**
 * Issue admin queue command
 *
 * @v intelxl		Intel device
 * @v cmd		Command descriptor
 * @ret rc		Return status code
 */
static int intelxl_admin_command ( struct intelxl_nic *intelxl,
				   struct intelxl_admin_descriptor *cmd ) {
	struct intelxl_admin *admin = &intelxl->command;
	void *admin_regs = ( intelxl->regs + admin->reg );
	struct intelxl_admin_descriptor *desc;
	uint64_t buffer;
	unsigned int index;
	unsigned int tail;
	unsigned int i;
	int rc;

	/* Get next queue entry */
	index = admin->index++;
	tail = ( admin->index % INTELXL_ADMIN_NUM_DESC );
	desc = &admin->desc[index % INTELXL_ADMIN_NUM_DESC];

	/* Clear must-be-zero flags */
	cmd->flags &= ~cpu_to_le16 ( INTELXL_ADMIN_FL_DD |
				     INTELXL_ADMIN_FL_CMP |
				     INTELXL_ADMIN_FL_ERR );

	/* Clear return value */
	cmd->ret = 0;

	/* Populate cookie */
	cmd->cookie = cpu_to_le32 ( index );

	/* Populate data buffer address if applicable */
	if ( cmd->flags & cpu_to_le16 ( INTELXL_ADMIN_FL_BUF ) ) {
		buffer = virt_to_bus ( admin->buffer );
		cmd->params.buffer.high = cpu_to_le32 ( buffer >> 32 );
		cmd->params.buffer.low = cpu_to_le32 ( buffer & 0xffffffffUL );
	}

	/* Copy command descriptor to queue entry */
	memcpy ( desc, cmd, sizeof ( *desc ) );
	DBGC2 ( intelxl, "INTELXL %p admin command %#x:\n", intelxl, index );
	DBGC2_HDA ( intelxl, virt_to_phys ( desc ), desc, sizeof ( *desc ) );

	/* Post command descriptor */
	wmb();
	writel ( tail, admin_regs + INTELXL_ADMIN_TAIL );

	/* Wait for completion */
	for ( i = 0 ; i < INTELXL_ADMIN_MAX_WAIT_MS ; i++ ) {

		/* If response is not complete, delay 1ms and retry */
		if ( ! ( desc->flags & INTELXL_ADMIN_FL_DD ) ) {
			mdelay ( 1 );
			continue;
		}
		DBGC2 ( intelxl, "INTELXL %p admin command %#x response:\n",
			intelxl, index );
		DBGC2_HDA ( intelxl, virt_to_phys ( desc ), desc,
			    sizeof ( *desc ) );

		/* Check for cookie mismatch */
		if ( desc->cookie != cmd->cookie ) {
			DBGC ( intelxl, "INTELXL %p admin command %#x bad "
			       "cookie %#x\n", intelxl, index,
			       le32_to_cpu ( desc->cookie ) );
			rc = -EPROTO;
			goto err;
		}

		/* Check for errors */
		if ( desc->ret != 0 ) {
			DBGC ( intelxl, "INTELXL %p admin command %#x error "
			       "%d\n", intelxl, index,
			       le16_to_cpu ( desc->ret ) );
			rc = -EIO;
			goto err;
		}

		/* Copy response back to command descriptor */
		memcpy ( cmd, desc, sizeof ( *cmd ) );

		/* Success */
		return 0;
	}

	rc = -ETIMEDOUT;
	DBGC ( intelxl, "INTELXL %p timed out waiting for admin command %#x:\n",
	       intelxl, index );
 err:
	DBGC_HDA ( intelxl, virt_to_phys ( desc ), cmd, sizeof ( *cmd ) );
	DBGC_HDA ( intelxl, virt_to_phys ( desc ), desc, sizeof ( *desc ) );
	return rc;
}

/**
 * Get firmware version
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int intelxl_admin_version ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin_descriptor cmd;
	struct intelxl_admin_version_params *version = &cmd.params.version;
	unsigned int api;
	int rc;

	/* Populate descriptor */
	memset ( &cmd, 0, sizeof ( cmd ) );
	cmd.opcode = cpu_to_le16 ( INTELXL_ADMIN_VERSION );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl, &cmd ) ) != 0 )
		return rc;
	api = le16_to_cpu ( version->api.major );
	DBGC ( intelxl, "INTELXL %p firmware v%d.%d API v%d.%d\n",
	       intelxl, le16_to_cpu ( version->firmware.major ),
	       le16_to_cpu ( version->firmware.minor ),
	       api, le16_to_cpu ( version->api.minor ) );

	/* Check for API compatibility */
	if ( api > INTELXL_ADMIN_API_MAJOR ) {
		DBGC ( intelxl, "INTELXL %p unsupported API v%d\n",
		       intelxl, api );
		return -ENOTSUP;
	}

	return 0;
}

/**
 * Report driver version
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int intelxl_admin_driver ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin_descriptor cmd;
	struct intelxl_admin_driver_params *driver = &cmd.params.driver;
	struct intelxl_admin_driver_buffer *buf =
		&intelxl->command.buffer->driver;
	int rc;

	/* Populate descriptor */
	memset ( &cmd, 0, sizeof ( cmd ) );
	cmd.opcode = cpu_to_le16 ( INTELXL_ADMIN_DRIVER );
	cmd.flags = cpu_to_le16 ( INTELXL_ADMIN_FL_RD | INTELXL_ADMIN_FL_BUF );
	cmd.len = cpu_to_le16 ( sizeof ( *buf ) );
	driver->major = product_major_version;
	driver->minor = product_minor_version;
	snprintf ( buf->name, sizeof ( buf->name ), "%s",
		   ( product_name[0] ? product_name : product_short_name ) );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl, &cmd ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Shutdown admin queues
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int intelxl_admin_shutdown ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin_descriptor cmd;
	struct intelxl_admin_shutdown_params *shutdown = &cmd.params.shutdown;
	int rc;

	/* Populate descriptor */
	memset ( &cmd, 0, sizeof ( cmd ) );
	cmd.opcode = cpu_to_le16 ( INTELXL_ADMIN_SHUTDOWN );
	shutdown->unloading = INTELXL_ADMIN_SHUTDOWN_UNLOADING;

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl, &cmd ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Get switch configuration
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int intelxl_admin_switch ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin_descriptor cmd;
	struct intelxl_admin_switch_params *sw = &cmd.params.sw;
	struct intelxl_admin_switch_buffer *buf = &intelxl->command.buffer->sw;
	struct intelxl_admin_switch_config *cfg = &buf->cfg;
	int rc;

	/* Populate descriptor */
	memset ( &cmd, 0, sizeof ( cmd ) );
	cmd.opcode = cpu_to_le16 ( INTELXL_ADMIN_SWITCH );
	cmd.flags = cpu_to_le16 ( INTELXL_ADMIN_FL_BUF );
	cmd.len = cpu_to_le16 ( sizeof ( *buf ) );

	/* Get each configuration in turn */
	do {
		/* Issue command */
		if ( ( rc = intelxl_admin_command ( intelxl, &cmd ) ) != 0 )
			return rc;

		/* Dump raw configuration */
		DBGC2 ( intelxl, "INTELXL %p SEID %#04x:\n",
			intelxl, le16_to_cpu ( cfg->seid ) );
		DBGC2_HDA ( intelxl, 0, cfg, sizeof ( *cfg ) );

		/* Parse response */
		if ( cfg->type == INTELXL_ADMIN_SWITCH_TYPE_VSI ) {
			intelxl->vsi = le16_to_cpu ( cfg->seid );
			DBGC ( intelxl, "INTELXL %p VSI %#04x uplink %#04x "
			       "downlink %#04x conn %#02x\n", intelxl,
			       intelxl->vsi, le16_to_cpu ( cfg->uplink ),
			       le16_to_cpu ( cfg->downlink ), cfg->connection );
		}

	} while ( sw->next );

	/* Check that we found a VSI */
	if ( ! intelxl->vsi ) {
		DBGC ( intelxl, "INTELXL %p has no VSI\n", intelxl );
		return -ENOENT;
	}

	return 0;
}

/**
 * Get VSI parameters
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int intelxl_admin_vsi ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin_descriptor cmd;
	struct intelxl_admin_vsi_params *vsi = &cmd.params.vsi;
	struct intelxl_admin_vsi_buffer *buf = &intelxl->command.buffer->vsi;
	int rc;

	/* Populate descriptor */
	memset ( &cmd, 0, sizeof ( cmd ) );
	cmd.opcode = cpu_to_le16 ( INTELXL_ADMIN_VSI );
	cmd.flags = cpu_to_le16 ( INTELXL_ADMIN_FL_BUF );
	cmd.len = cpu_to_le16 ( sizeof ( *buf ) );
	vsi->vsi = cpu_to_le16 ( intelxl->vsi );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl, &cmd ) ) != 0 )
		return rc;

	/* Parse response */
	intelxl->queue = le16_to_cpu ( buf->queue[0] );
	intelxl->qset = le16_to_cpu ( buf->qset[0] );
	DBGC ( intelxl, "INTELXL %p VSI %#04x queue %#04x qset %#04x\n",
	       intelxl, intelxl->vsi, intelxl->queue, intelxl->qset );

	return 0;
}

/**
 * Set VSI promiscuous modes
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int intelxl_admin_promisc ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin_descriptor cmd;
	struct intelxl_admin_promisc_params *promisc = &cmd.params.promisc;
	uint16_t flags;
	int rc;

	/* Populate descriptor */
	memset ( &cmd, 0, sizeof ( cmd ) );
	cmd.opcode = cpu_to_le16 ( INTELXL_ADMIN_PROMISC );
	flags = ( INTELXL_ADMIN_PROMISC_FL_UNICAST |
		  INTELXL_ADMIN_PROMISC_FL_MULTICAST |
		  INTELXL_ADMIN_PROMISC_FL_BROADCAST |
		  INTELXL_ADMIN_PROMISC_FL_VLAN );
	promisc->flags = cpu_to_le16 ( flags );
	promisc->valid = cpu_to_le16 ( flags );
	promisc->vsi = cpu_to_le16 ( intelxl->vsi );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl, &cmd ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Restart autonegotiation
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int intelxl_admin_autoneg ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin_descriptor cmd;
	struct intelxl_admin_autoneg_params *autoneg = &cmd.params.autoneg;
	int rc;

	/* Populate descriptor */
	memset ( &cmd, 0, sizeof ( cmd ) );
	cmd.opcode = cpu_to_le16 ( INTELXL_ADMIN_AUTONEG );
	autoneg->flags = ( INTELXL_ADMIN_AUTONEG_FL_RESTART |
			   INTELXL_ADMIN_AUTONEG_FL_ENABLE );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl, &cmd ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Get link status
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int intelxl_admin_link ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct intelxl_admin_descriptor cmd;
	struct intelxl_admin_link_params *link = &cmd.params.link;
	int rc;

	/* Populate descriptor */
	memset ( &cmd, 0, sizeof ( cmd ) );
	cmd.opcode = cpu_to_le16 ( INTELXL_ADMIN_LINK );
	link->notify = INTELXL_ADMIN_LINK_NOTIFY;

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl, &cmd ) ) != 0 )
		return rc;
	DBGC ( intelxl, "INTELXL %p PHY %#02x speed %#02x status %#02x\n",
	       intelxl, link->phy, link->speed, link->status );

	/* Update network device */
	if ( link->status & INTELXL_ADMIN_LINK_UP ) {
		netdev_link_up ( netdev );
	} else {
		netdev_link_down ( netdev );
	}

	return 0;
}

/**
 * Refill admin event queue
 *
 * @v intelxl		Intel device
 */
static void intelxl_refill_admin ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin *admin = &intelxl->event;
	void *admin_regs = ( intelxl->regs + admin->reg );
	unsigned int tail;

	/* Update tail pointer */
	tail = ( ( admin->index + INTELXL_ADMIN_NUM_DESC - 1 ) %
		 INTELXL_ADMIN_NUM_DESC );
	writel ( tail, admin_regs + INTELXL_ADMIN_TAIL );
}

/**
 * Poll admin event queue
 *
 * @v netdev		Network device
 */
static void intelxl_poll_admin ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct intelxl_admin *admin = &intelxl->event;
	struct intelxl_admin_descriptor *desc;

	/* Check for events */
	while ( 1 ) {

		/* Get next event descriptor */
		desc = &admin->desc[admin->index % INTELXL_ADMIN_NUM_DESC];

		/* Stop if descriptor is not yet completed */
		if ( ! ( desc->flags & INTELXL_ADMIN_FL_DD ) )
			return;
		DBGC2 ( intelxl, "INTELXL %p admin event %#x:\n",
			intelxl, admin->index );
		DBGC2_HDA ( intelxl, virt_to_phys ( desc ), desc,
			    sizeof ( *desc ) );

		/* Handle event */
		switch ( desc->opcode ) {
		case cpu_to_le16 ( INTELXL_ADMIN_LINK ):
			intelxl_admin_link ( netdev );
			break;
		default:
			DBGC ( intelxl, "INTELXL %p admin event %#x "
			       "unrecognised opcode %#04x\n", intelxl,
			       admin->index, le16_to_cpu ( desc->opcode ) );
			break;
		}

		/* Clear event completion flag */
		desc->flags = 0;
		wmb();

		/* Update index and refill queue */
		admin->index++;
		intelxl_refill_admin ( intelxl );
	}
}

/**
 * Open admin queues
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int intelxl_open_admin ( struct intelxl_nic *intelxl ) {
	int rc;

	/* Create admin event queue */
	if ( ( rc = intelxl_create_admin ( intelxl, &intelxl->event ) ) != 0 )
		goto err_create_event;

	/* Create admin command queue */
	if ( ( rc = intelxl_create_admin ( intelxl, &intelxl->command ) ) != 0 )
		goto err_create_command;

	/* Post all descriptors to event queue */
	intelxl_refill_admin ( intelxl );

	/* Get firmware version */
	if ( ( rc = intelxl_admin_version ( intelxl ) ) != 0 )
		goto err_version;

	/* Report driver version */
	if ( ( rc = intelxl_admin_driver ( intelxl ) ) != 0 )
		goto err_driver;

	return 0;

 err_driver:
 err_version:
	intelxl_destroy_admin ( intelxl, &intelxl->command );
 err_create_command:
	intelxl_destroy_admin ( intelxl, &intelxl->event );
 err_create_event:
	return rc;
}

/**
 * Close admin queues
 *
 * @v intelxl		Intel device
 */
static void intelxl_close_admin ( struct intelxl_nic *intelxl ) {

	/* Shut down admin queues */
	intelxl_admin_shutdown ( intelxl );

	/* Destroy admin command queue */
	intelxl_destroy_admin ( intelxl, &intelxl->command );

	/* Destroy admin event queue */
	intelxl_destroy_admin ( intelxl, &intelxl->event );
}

/******************************************************************************
 *
 * Descriptor rings
 *
 ******************************************************************************
 */

/**
 * Dump queue context (for debugging)
 *
 * @v intelxl		Intel device
 * @v op		Context operation
 * @v len		Size of context
 */
static __attribute__ (( unused )) void
intelxl_context_dump ( struct intelxl_nic *intelxl, uint32_t op, size_t len ) {
	struct intelxl_context_line line;
	uint32_t pfcm_lanctxctl;
	uint32_t pfcm_lanctxstat;
	unsigned int queue;
	unsigned int index;
	unsigned int i;

	/* Do nothing unless debug output is enabled */
	if ( ! DBG_EXTRA )
		return;

	/* Dump context */
	DBGC2 ( intelxl, "INTELXL %p context %#08x:\n", intelxl, op );
	for ( index = 0 ; ( sizeof ( line ) * index ) < len ; index++ ) {

		/* Start context operation */
		queue = ( intelxl->base + intelxl->queue );
		pfcm_lanctxctl =
			( INTELXL_PFCM_LANCTXCTL_QUEUE_NUM ( queue ) |
			  INTELXL_PFCM_LANCTXCTL_SUB_LINE ( index ) |
			  INTELXL_PFCM_LANCTXCTL_OP_CODE_READ | op );
		writel ( pfcm_lanctxctl,
			 intelxl->regs + INTELXL_PFCM_LANCTXCTL );

		/* Wait for operation to complete */
		for ( i = 0 ; i < INTELXL_CTX_MAX_WAIT_MS ; i++ ) {

			/* Check if operation is complete */
			pfcm_lanctxstat = readl ( intelxl->regs +
						  INTELXL_PFCM_LANCTXSTAT );
			if ( pfcm_lanctxstat & INTELXL_PFCM_LANCTXSTAT_DONE )
				break;

			/* Delay */
			mdelay ( 1 );
		}

		/* Read context data */
		for ( i = 0 ; i < ( sizeof ( line ) /
				    sizeof ( line.raw[0] ) ) ; i++ ) {
			line.raw[i] = readl ( intelxl->regs +
					      INTELXL_PFCM_LANCTXDATA ( i ) );
		}
		DBGC2_HDA ( intelxl, ( sizeof ( line ) * index ),
			    &line, sizeof ( line ) );
	}
}

/**
 * Program queue context line
 *
 * @v intelxl		Intel device
 * @v line		Queue context line
 * @v index		Line number
 * @v op		Context operation
 * @ret rc		Return status code
 */
static int intelxl_context_line ( struct intelxl_nic *intelxl,
				  struct intelxl_context_line *line,
				  unsigned int index, uint32_t op ) {
	uint32_t pfcm_lanctxctl;
	uint32_t pfcm_lanctxstat;
	unsigned int queue;
	unsigned int i;

	/* Write context data */
	for ( i = 0; i < ( sizeof ( *line ) / sizeof ( line->raw[0] ) ); i++ ) {
		writel ( le32_to_cpu ( line->raw[i] ),
			 intelxl->regs + INTELXL_PFCM_LANCTXDATA ( i ) );
	}

	/* Start context operation */
	queue = ( intelxl->base + intelxl->queue );
	pfcm_lanctxctl = ( INTELXL_PFCM_LANCTXCTL_QUEUE_NUM ( queue ) |
			   INTELXL_PFCM_LANCTXCTL_SUB_LINE ( index ) |
			   INTELXL_PFCM_LANCTXCTL_OP_CODE_WRITE | op );
	writel ( pfcm_lanctxctl, intelxl->regs + INTELXL_PFCM_LANCTXCTL );

	/* Wait for operation to complete */
	for ( i = 0 ; i < INTELXL_CTX_MAX_WAIT_MS ; i++ ) {

		/* Check if operation is complete */
		pfcm_lanctxstat = readl ( intelxl->regs +
					  INTELXL_PFCM_LANCTXSTAT );
		if ( pfcm_lanctxstat & INTELXL_PFCM_LANCTXSTAT_DONE )
			return 0;

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( intelxl, "INTELXL %p timed out waiting for context: %#08x\n",
	       intelxl, pfcm_lanctxctl );
	return -ETIMEDOUT;
}

/**
 * Program queue context
 *
 * @v intelxl		Intel device
 * @v line		Queue context lines
 * @v len		Size of context
 * @v op		Context operation
 * @ret rc		Return status code
 */
static int intelxl_context ( struct intelxl_nic *intelxl,
			     struct intelxl_context_line *line,
			     size_t len, uint32_t op ) {
	unsigned int index;
	int rc;

	DBGC2 ( intelxl, "INTELXL %p context %#08x len %#zx:\n",
		intelxl, op, len );
	DBGC2_HDA ( intelxl, 0, line, len );

	/* Program one line at a time */
	for ( index = 0 ; ( sizeof ( *line ) * index ) < len ; index++ ) {
		if ( ( rc = intelxl_context_line ( intelxl, line++, index,
						   op ) ) != 0 )
			return rc;
	}

	return 0;
}

/**
 * Program transmit queue context
 *
 * @v intelxl		Intel device
 * @v address		Descriptor ring base address
 * @ret rc		Return status code
 */
static int intelxl_context_tx ( struct intelxl_nic *intelxl,
				physaddr_t address ) {
	union {
		struct intelxl_context_tx tx;
		struct intelxl_context_line line;
	} ctx;
	int rc;

	/* Initialise context */
	memset ( &ctx, 0, sizeof ( ctx ) );
	ctx.tx.flags = cpu_to_le16 ( INTELXL_CTX_TX_FL_NEW );
	ctx.tx.base = cpu_to_le64 ( INTELXL_CTX_TX_BASE ( address ) );
	ctx.tx.count =
		cpu_to_le16 ( INTELXL_CTX_TX_COUNT ( INTELXL_TX_NUM_DESC ) );
	ctx.tx.qset = INTELXL_CTX_TX_QSET ( intelxl->qset );

	/* Program context */
	if ( ( rc = intelxl_context ( intelxl, &ctx.line, sizeof ( ctx ),
				      INTELXL_PFCM_LANCTXCTL_TYPE_TX ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Program receive queue context
 *
 * @v intelxl		Intel device
 * @v address		Descriptor ring base address
 * @ret rc		Return status code
 */
static int intelxl_context_rx ( struct intelxl_nic *intelxl,
				physaddr_t address ) {
	union {
		struct intelxl_context_rx rx;
		struct intelxl_context_line line;
	} ctx;
	uint64_t base_count;
	int rc;

	/* Initialise context */
	memset ( &ctx, 0, sizeof ( ctx ) );
	base_count = INTELXL_CTX_RX_BASE_COUNT ( address, INTELXL_RX_NUM_DESC );
	ctx.rx.base_count = cpu_to_le64 ( base_count );
	ctx.rx.len = cpu_to_le16 ( INTELXL_CTX_RX_LEN ( intelxl->mfs ) );
	ctx.rx.flags = INTELXL_CTX_RX_FL_CRCSTRIP;
	ctx.rx.mfs = cpu_to_le16 ( INTELXL_CTX_RX_MFS ( intelxl->mfs ) );

	/* Program context */
	if ( ( rc = intelxl_context ( intelxl, &ctx.line, sizeof ( ctx ),
				      INTELXL_PFCM_LANCTXCTL_TYPE_RX ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Enable descriptor ring
 *
 * @v intelxl		Intel device
 * @v ring		Descriptor ring
 * @ret rc		Return status code
 */
static int intelxl_enable_ring ( struct intelxl_nic *intelxl,
				 struct intelxl_ring *ring ) {
	void *ring_regs = ( intelxl->regs + ring->reg );
	uint32_t qxx_ena;

	/* Enable ring */
	writel ( INTELXL_QXX_ENA_REQ, ( ring_regs + INTELXL_QXX_ENA ) );
	udelay ( INTELXL_QUEUE_ENABLE_DELAY_US );
	qxx_ena = readl ( ring_regs + INTELXL_QXX_ENA );
	if ( ! ( qxx_ena & INTELXL_QXX_ENA_STAT ) ) {
		DBGC ( intelxl, "INTELXL %p ring %06x failed to enable: "
		       "%#08x\n", intelxl, ring->reg, qxx_ena );
		return -EIO;
	}

	return 0;
}

/**
 * Disable descriptor ring
 *
 * @v intelxl		Intel device
 * @v ring		Descriptor ring
 * @ret rc		Return status code
 */
static int intelxl_disable_ring ( struct intelxl_nic *intelxl,
				  struct intelxl_ring *ring ) {
	void *ring_regs = ( intelxl->regs + ring->reg );
	uint32_t qxx_ena;
	unsigned int i;

	/* Disable ring */
	writel ( 0, ( ring_regs + INTELXL_QXX_ENA ) );

	/* Wait for ring to be disabled */
	for ( i = 0 ; i < INTELXL_QUEUE_DISABLE_MAX_WAIT_MS ; i++ ) {

		/* Check if ring is disabled */
		qxx_ena = readl ( ring_regs + INTELXL_QXX_ENA );
		if ( ! ( qxx_ena & INTELXL_QXX_ENA_STAT ) )
			return 0;

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( intelxl, "INTELXL %p ring %06x timed out waiting for disable: "
	       "%#08x\n", intelxl, ring->reg, qxx_ena );
	return -ETIMEDOUT;
}

/**
 * Create descriptor ring
 *
 * @v intelxl		Intel device
 * @v ring		Descriptor ring
 * @ret rc		Return status code
 */
static int intelxl_create_ring ( struct intelxl_nic *intelxl,
				 struct intelxl_ring *ring ) {
	void *ring_regs = ( intelxl->regs + ring->reg );
	physaddr_t address;
	int rc;

	/* Allocate descriptor ring */
	ring->desc = malloc_dma ( ring->len, INTELXL_ALIGN );
	if ( ! ring->desc ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Initialise descriptor ring */
	memset ( ring->desc, 0, ring->len );

	/* Reset tail pointer */
	writel ( 0, ( ring_regs + INTELXL_QXX_TAIL ) );

	/* Program queue context */
	address = virt_to_bus ( ring->desc );
	if ( ( rc = ring->context ( intelxl, address ) ) != 0 )
		goto err_context;

	/* Enable ring */
	if ( ( rc = intelxl_enable_ring ( intelxl, ring ) ) != 0 )
		goto err_enable;

	/* Reset counters */
	ring->prod = 0;
	ring->cons = 0;

	DBGC ( intelxl, "INTELXL %p ring %06x is at [%08llx,%08llx)\n",
	       intelxl, ring->reg, ( ( unsigned long long ) address ),
	       ( ( unsigned long long ) address + ring->len ) );

	return 0;

	intelxl_disable_ring ( intelxl, ring );
 err_enable:
 err_context:
	free_dma ( ring->desc, ring->len );
 err_alloc:
	return rc;
}

/**
 * Destroy descriptor ring
 *
 * @v intelxl		Intel device
 * @v ring		Descriptor ring
 */
static void intelxl_destroy_ring ( struct intelxl_nic *intelxl,
				   struct intelxl_ring *ring ) {
	int rc;

	/* Disable ring */
	if ( ( rc = intelxl_disable_ring ( intelxl, ring ) ) != 0 ) {
		/* Leak memory; there's nothing else we can do */
		return;
	}

	/* Free descriptor ring */
	free_dma ( ring->desc, ring->len );
	ring->desc = NULL;
}

/**
 * Refill receive descriptor ring
 *
 * @v intelxl		Intel device
 */
static void intelxl_refill_rx ( struct intelxl_nic *intelxl ) {
	struct intelxl_rx_data_descriptor *rx;
	struct io_buffer *iobuf;
	unsigned int rx_idx;
	unsigned int rx_tail;
	physaddr_t address;
	unsigned int refilled = 0;

	/* Refill ring */
	while ( ( intelxl->rx.prod - intelxl->rx.cons ) < INTELXL_RX_FILL ) {

		/* Allocate I/O buffer */
		iobuf = alloc_iob ( intelxl->mfs );
		if ( ! iobuf ) {
			/* Wait for next refill */
			break;
		}

		/* Get next receive descriptor */
		rx_idx = ( intelxl->rx.prod++ % INTELXL_RX_NUM_DESC );
		rx = &intelxl->rx.desc[rx_idx].rx;

		/* Populate receive descriptor */
		address = virt_to_bus ( iobuf->data );
		rx->address = cpu_to_le64 ( address );
		rx->flags = 0;

		/* Record I/O buffer */
		assert ( intelxl->rx_iobuf[rx_idx] == NULL );
		intelxl->rx_iobuf[rx_idx] = iobuf;

		DBGC2 ( intelxl, "INTELXL %p RX %d is [%llx,%llx)\n", intelxl,
			rx_idx, ( ( unsigned long long ) address ),
			( ( unsigned long long ) address + intelxl->mfs ) );
		refilled++;
	}

	/* Push descriptors to card, if applicable */
	if ( refilled ) {
		wmb();
		rx_tail = ( intelxl->rx.prod % INTELXL_RX_NUM_DESC );
		writel ( rx_tail,
			 ( intelxl->regs + intelxl->rx.reg + INTELXL_QXX_TAIL));
	}
}

/******************************************************************************
 *
 * Network device interface
 *
 ******************************************************************************
 */

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int intelxl_open ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	union intelxl_receive_address mac;
	unsigned int queue;
	uint32_t prtgl_sal;
	uint32_t prtgl_sah;
	int rc;

	/* Calculate maximum frame size */
	intelxl->mfs = ( ( ETH_HLEN + netdev->mtu + 4 /* CRC */ +
			   INTELXL_ALIGN - 1 ) & ~( INTELXL_ALIGN - 1 ) );

	/* Program MAC address and maximum frame size */
	memset ( &mac, 0, sizeof ( mac ) );
	memcpy ( mac.raw, netdev->ll_addr, sizeof ( mac.raw ) );
	prtgl_sal = le32_to_cpu ( mac.reg.low );
	prtgl_sah = ( le32_to_cpu ( mac.reg.high ) |
		      INTELXL_PRTGL_SAH_MFS_SET ( intelxl->mfs ) );
	writel ( prtgl_sal, intelxl->regs + INTELXL_PRTGL_SAL );
	writel ( prtgl_sah, intelxl->regs + INTELXL_PRTGL_SAH );

	/* Associate transmit queue to PF */
	writel ( ( INTELXL_QXX_CTL_PFVF_Q_PF |
		   INTELXL_QXX_CTL_PFVF_PF_INDX ( intelxl->pf ) ),
		 ( intelxl->regs + intelxl->tx.reg + INTELXL_QXX_CTL ) );

	/* Clear transmit pre queue disable */
	queue = ( intelxl->base + intelxl->queue );
	writel ( ( INTELXL_GLLAN_TXPRE_QDIS_CLEAR_QDIS |
		   INTELXL_GLLAN_TXPRE_QDIS_QINDX ( queue ) ),
		 ( intelxl->regs + INTELXL_GLLAN_TXPRE_QDIS ( queue ) ) );

	/* Reset transmit queue head */
	writel ( 0, ( intelxl->regs + INTELXL_QTX_HEAD ( intelxl->queue ) ) );

	/* Create receive descriptor ring */
	if ( ( rc = intelxl_create_ring ( intelxl, &intelxl->rx ) ) != 0 )
		goto err_create_rx;

	/* Create transmit descriptor ring */
	if ( ( rc = intelxl_create_ring ( intelxl, &intelxl->tx ) ) != 0 )
		goto err_create_tx;

	/* Fill receive ring */
	intelxl_refill_rx ( intelxl );

	/* Restart autonegotiation */
	intelxl_admin_autoneg ( intelxl );

	/* Update link state */
	intelxl_admin_link ( netdev );

	return 0;

	writel ( ( INTELXL_GLLAN_TXPRE_QDIS_SET_QDIS |
		   INTELXL_GLLAN_TXPRE_QDIS_QINDX ( queue ) ),
		 ( intelxl->regs + INTELXL_GLLAN_TXPRE_QDIS ( queue ) ) );
	udelay ( INTELXL_QUEUE_PRE_DISABLE_DELAY_US );
	intelxl_destroy_ring ( intelxl, &intelxl->tx );
 err_create_tx:
	intelxl_destroy_ring ( intelxl, &intelxl->rx );
 err_create_rx:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void intelxl_close ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	unsigned int queue;
	unsigned int i;

	/* Dump contexts (for debugging) */
	intelxl_context_dump ( intelxl, INTELXL_PFCM_LANCTXCTL_TYPE_TX,
			       sizeof ( struct intelxl_context_tx ) );
	intelxl_context_dump ( intelxl, INTELXL_PFCM_LANCTXCTL_TYPE_RX,
			       sizeof ( struct intelxl_context_rx ) );

	/* Pre-disable transmit queue */
	queue = ( intelxl->base + intelxl->queue );
	writel ( ( INTELXL_GLLAN_TXPRE_QDIS_SET_QDIS |
		   INTELXL_GLLAN_TXPRE_QDIS_QINDX ( queue ) ),
		 ( intelxl->regs + INTELXL_GLLAN_TXPRE_QDIS ( queue ) ) );
	udelay ( INTELXL_QUEUE_PRE_DISABLE_DELAY_US );

	/* Destroy transmit descriptor ring */
	intelxl_destroy_ring ( intelxl, &intelxl->tx );

	/* Destroy receive descriptor ring */
	intelxl_destroy_ring ( intelxl, &intelxl->rx );

	/* Discard any unused receive buffers */
	for ( i = 0 ; i < INTELXL_RX_NUM_DESC ; i++ ) {
		if ( intelxl->rx_iobuf[i] )
			free_iob ( intelxl->rx_iobuf[i] );
		intelxl->rx_iobuf[i] = NULL;
	}
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int intelxl_transmit ( struct net_device *netdev,
			      struct io_buffer *iobuf ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct intelxl_tx_data_descriptor *tx;
	unsigned int tx_idx;
	unsigned int tx_tail;
	physaddr_t address;
	size_t len;

	/* Get next transmit descriptor */
	if ( ( intelxl->tx.prod - intelxl->tx.cons ) >= INTELXL_TX_FILL ) {
		DBGC ( intelxl, "INTELXL %p out of transmit descriptors\n",
		       intelxl );
		return -ENOBUFS;
	}
	tx_idx = ( intelxl->tx.prod++ % INTELXL_TX_NUM_DESC );
	tx_tail = ( intelxl->tx.prod % INTELXL_TX_NUM_DESC );
	tx = &intelxl->tx.desc[tx_idx].tx;

	/* Populate transmit descriptor */
	address = virt_to_bus ( iobuf->data );
	len = iob_len ( iobuf );
	tx->address = cpu_to_le64 ( address );
	tx->len = cpu_to_le32 ( INTELXL_TX_DATA_LEN ( len ) );
	tx->flags = cpu_to_le32 ( INTELXL_TX_DATA_DTYP | INTELXL_TX_DATA_EOP |
				  INTELXL_TX_DATA_RS | INTELXL_TX_DATA_JFDI );
	wmb();

	/* Notify card that there are packets ready to transmit */
	writel ( tx_tail,
		 ( intelxl->regs + intelxl->tx.reg + INTELXL_QXX_TAIL ) );

	DBGC2 ( intelxl, "INTELXL %p TX %d is [%llx,%llx)\n", intelxl, tx_idx,
		( ( unsigned long long ) address ),
		( ( unsigned long long ) address + len ) );
	return 0;
}

/**
 * Poll for completed packets
 *
 * @v netdev		Network device
 */
static void intelxl_poll_tx ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct intelxl_tx_writeback_descriptor *tx_wb;
	unsigned int tx_idx;

	/* Check for completed packets */
	while ( intelxl->tx.cons != intelxl->tx.prod ) {

		/* Get next transmit descriptor */
		tx_idx = ( intelxl->tx.cons % INTELXL_TX_NUM_DESC );
		tx_wb = &intelxl->tx.desc[tx_idx].tx_wb;

		/* Stop if descriptor is still in use */
		if ( ! ( tx_wb->flags & INTELXL_TX_WB_FL_DD ) )
			return;
		DBGC2 ( intelxl, "INTELXL %p TX %d complete\n",
			intelxl, tx_idx );

		/* Complete TX descriptor */
		netdev_tx_complete_next ( netdev );
		intelxl->tx.cons++;
	}
}

/**
 * Poll for received packets
 *
 * @v netdev		Network device
 */
static void intelxl_poll_rx ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct intelxl_rx_writeback_descriptor *rx_wb;
	struct io_buffer *iobuf;
	unsigned int rx_idx;
	size_t len;

	/* Check for received packets */
	while ( intelxl->rx.cons != intelxl->rx.prod ) {

		/* Get next receive descriptor */
		rx_idx = ( intelxl->rx.cons % INTELXL_RX_NUM_DESC );
		rx_wb = &intelxl->rx.desc[rx_idx].rx_wb;

		/* Stop if descriptor is still in use */
		if ( ! ( rx_wb->flags & cpu_to_le32 ( INTELXL_RX_WB_FL_DD ) ) )
			return;

		/* Populate I/O buffer */
		iobuf = intelxl->rx_iobuf[rx_idx];
		intelxl->rx_iobuf[rx_idx] = NULL;
		len = INTELXL_RX_WB_LEN ( le32_to_cpu ( rx_wb->len ) );
		iob_put ( iobuf, len );

		/* Hand off to network stack */
		if ( rx_wb->flags & cpu_to_le32 ( INTELXL_RX_WB_FL_RXE ) ) {
			DBGC ( intelxl, "INTELXL %p RX %d error (length %zd, "
			       "flags %08x)\n", intelxl, rx_idx, len,
			       le32_to_cpu ( rx_wb->flags ) );
			netdev_rx_err ( netdev, iobuf, -EIO );
		} else {
			DBGC2 ( intelxl, "INTELXL %p RX %d complete (length "
				"%zd)\n", intelxl, rx_idx, len );
			netdev_rx ( netdev, iobuf );
		}
		intelxl->rx.cons++;
	}
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void intelxl_poll ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;

	/* Acknowledge interrupts, if applicable */
	if ( netdev_irq_enabled ( netdev ) ) {
		writel ( ( INTELXL_PFINT_DYN_CTL0_CLEARPBA |
			   INTELXL_PFINT_DYN_CTL0_INTENA_MASK ),
			 intelxl->regs + INTELXL_PFINT_DYN_CTL0 );
	}

	/* Poll for completed packets */
	intelxl_poll_tx ( netdev );

	/* Poll for received packets */
	intelxl_poll_rx ( netdev );

	/* Poll for admin events */
	intelxl_poll_admin ( netdev );

	/* Refill RX ring */
	intelxl_refill_rx ( intelxl );
}

/**
 * Enable or disable interrupts
 *
 * @v netdev		Network device
 * @v enable		Interrupts should be enabled
 */
static void intelxl_irq ( struct net_device *netdev, int enable ) {
	struct intelxl_nic *intelxl = netdev->priv;

	if ( enable ) {
		writel ( INTELXL_PFINT_DYN_CTL0_INTENA,
			 intelxl->regs + INTELXL_PFINT_DYN_CTL0 );
	} else {
		writel ( 0, intelxl->regs + INTELXL_PFINT_DYN_CTL0 );
	}
}

/** Network device operations */
static struct net_device_operations intelxl_operations = {
	.open		= intelxl_open,
	.close		= intelxl_close,
	.transmit	= intelxl_transmit,
	.poll		= intelxl_poll,
	.irq		= intelxl_irq,
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
static int intelxl_probe ( struct pci_device *pci ) {
	struct net_device *netdev;
	struct intelxl_nic *intelxl;
	uint32_t pfgen_portnum;
	uint32_t pflan_qalloc;
	int rc;

	/* Allocate and initialise net device */
	netdev = alloc_etherdev ( sizeof ( *intelxl ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &intelxl_operations );
	intelxl = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	netdev->dev = &pci->dev;
	memset ( intelxl, 0, sizeof ( *intelxl ) );
	intelxl->pf = PCI_FUNC ( pci->busdevfn );
	intelxl_init_admin ( &intelxl->command, INTELXL_ADMIN_CMD );
	intelxl_init_admin ( &intelxl->event, INTELXL_ADMIN_EVT );
	intelxl_init_ring ( &intelxl->tx, INTELXL_TX_NUM_DESC,
			    intelxl_context_tx );
	intelxl_init_ring ( &intelxl->rx, INTELXL_RX_NUM_DESC,
			    intelxl_context_rx );

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Map registers */
	intelxl->regs = ioremap ( pci->membase, INTELXL_BAR_SIZE );
	if ( ! intelxl->regs ) {
		rc = -ENODEV;
		goto err_ioremap;
	}

	/* Reset the NIC */
	if ( ( rc = intelxl_reset ( intelxl ) ) != 0 )
		goto err_reset;

	/* Get port number and base queue number */
	pfgen_portnum = readl ( intelxl->regs + INTELXL_PFGEN_PORTNUM );
	intelxl->port = INTELXL_PFGEN_PORTNUM_PORT_NUM ( pfgen_portnum );
	pflan_qalloc = readl ( intelxl->regs + INTELXL_PFLAN_QALLOC );
	intelxl->base = INTELXL_PFLAN_QALLOC_FIRSTQ ( pflan_qalloc );
	DBGC ( intelxl, "INTELXL %p PF %d using port %d queues [%#04x-%#04x]\n",
	       intelxl, intelxl->pf, intelxl->port, intelxl->base,
	       INTELXL_PFLAN_QALLOC_LASTQ ( pflan_qalloc ) );

	/* Fetch MAC address and maximum frame size */
	if ( ( rc = intelxl_fetch_mac ( intelxl, netdev ) ) != 0 )
		goto err_fetch_mac;

	/* Open admin queues */
	if ( ( rc = intelxl_open_admin ( intelxl ) ) != 0 )
		goto err_open_admin;

	/* Get switch configuration */
	if ( ( rc = intelxl_admin_switch ( intelxl ) ) != 0 )
		goto err_admin_switch;

	/* Get VSI configuration */
	if ( ( rc = intelxl_admin_vsi ( intelxl ) ) != 0 )
		goto err_admin_vsi;

	/* Configure switch for promiscuous mode */
	if ( ( rc = intelxl_admin_promisc ( intelxl ) ) != 0 )
		goto err_admin_promisc;

	/* Configure queue register addresses */
	intelxl->tx.reg = INTELXL_QTX ( intelxl->queue );
	intelxl->rx.reg = INTELXL_QRX ( intelxl->queue );

	/* Configure interrupt causes */
	writel ( ( INTELXL_QINT_TQCTL_NEXTQ_INDX_NONE |
		   INTELXL_QINT_TQCTL_CAUSE_ENA ),
		 intelxl->regs + INTELXL_QINT_TQCTL ( intelxl->queue ) );
	writel ( ( INTELXL_QINT_RQCTL_NEXTQ_INDX ( intelxl->queue ) |
		   INTELXL_QINT_RQCTL_NEXTQ_TYPE_TX |
		   INTELXL_QINT_RQCTL_CAUSE_ENA ),
		 intelxl->regs + INTELXL_QINT_RQCTL ( intelxl->queue ) );
	writel ( ( INTELXL_PFINT_LNKLST0_FIRSTQ_INDX ( intelxl->queue ) |
		   INTELXL_PFINT_LNKLST0_FIRSTQ_TYPE_RX ),
		 intelxl->regs + INTELXL_PFINT_LNKLST0 );
	writel ( INTELXL_PFINT_ICR0_ENA_ADMINQ,
		 intelxl->regs + INTELXL_PFINT_ICR0_ENA );

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	/* Set initial link state */
	intelxl_admin_link ( netdev );

	return 0;

	unregister_netdev ( netdev );
 err_register_netdev:
 err_admin_promisc:
 err_admin_vsi:
 err_admin_switch:
	intelxl_close_admin ( intelxl );
 err_open_admin:
 err_fetch_mac:
	intelxl_reset ( intelxl );
 err_reset:
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
static void intelxl_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct intelxl_nic *intelxl = netdev->priv;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Close admin queues */
	intelxl_close_admin ( intelxl );

	/* Reset the NIC */
	intelxl_reset ( intelxl );

	/* Free network device */
	iounmap ( intelxl->regs );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** PCI device IDs */
static struct pci_device_id intelxl_nics[] = {
	PCI_ROM ( 0x8086, 0x1572, "x710-sfp", "X710 10GbE SFP+", 0 ),
	PCI_ROM ( 0x8086, 0x1574, "xl710-qemu", "Virtual XL710", 0 ),
	PCI_ROM ( 0x8086, 0x1580, "xl710-kx-b", "XL710 40GbE backplane", 0 ),
	PCI_ROM ( 0x8086, 0x1581, "xl710-kx-c", "XL710 10GbE backplane", 0 ),
	PCI_ROM ( 0x8086, 0x1583, "xl710-qda2", "XL710 40GbE QSFP+", 0 ),
	PCI_ROM ( 0x8086, 0x1584, "xl710-qda1", "XL710 40GbE QSFP+", 0 ),
	PCI_ROM ( 0x8086, 0x1585, "x710-qsfp", "X710 10GbE QSFP+", 0 ),
	PCI_ROM ( 0x8086, 0x1586, "x710-10gt", "X710 10GBASE-T", 0 ),
	PCI_ROM ( 0x8086, 0x1587, "x710-kr2", "XL710 20GbE backplane", 0 ),
	PCI_ROM ( 0x8086, 0x1588, "x710-kr2-a", "XL710 20GbE backplane", 0 ),
	PCI_ROM ( 0x8086, 0x1589, "x710-10gt4", "X710 10GBASE-T4", 0 ),
	PCI_ROM ( 0x8086, 0x158a, "xxv710", "XXV710 25GbE backplane", 0 ),
	PCI_ROM ( 0x8086, 0x158b, "xxv710-sfp28", "XXV710 25GbE SFP28", 0 ),
	PCI_ROM ( 0x8086, 0x37ce, "x722-kx", "X722 10GbE backplane", 0 ),
	PCI_ROM ( 0x8086, 0x37cf, "x722-qsfp", "X722 10GbE QSFP+", 0 ),
	PCI_ROM ( 0x8086, 0x37d0, "x722-sfp", "X722 10GbE SFP+", 0 ),
	PCI_ROM ( 0x8086, 0x37d1, "x722-1gt", "X722 1GBASE-T", 0 ),
	PCI_ROM ( 0x8086, 0x37d2, "x722-10gt", "X722 10GBASE-T", 0 ),
	PCI_ROM ( 0x8086, 0x37d3, "x722-sfp-i", "X722 10GbE SFP+", 0 ),
};

/** PCI driver */
struct pci_driver intelxl_driver __pci_driver = {
	.ids = intelxl_nics,
	.id_count = ( sizeof ( intelxl_nics ) / sizeof ( intelxl_nics[0] ) ),
	.probe = intelxl_probe,
	.remove = intelxl_remove,
};
