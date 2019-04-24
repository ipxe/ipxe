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
#include <ipxe/vlan.h>
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
 * MSI-X interrupts
 *
 ******************************************************************************
 */

/**
 * Enable MSI-X dummy interrupt
 *
 * @v intelxl		Intel device
 * @v pci		PCI device
 * @ret rc		Return status code
 */
int intelxl_msix_enable ( struct intelxl_nic *intelxl,
			  struct pci_device *pci ) {
	int rc;

	/* Enable MSI-X capability */
	if ( ( rc = pci_msix_enable ( pci, &intelxl->msix ) ) != 0 ) {
		DBGC ( intelxl, "INTELXL %p could not enable MSI-X: %s\n",
		       intelxl, strerror ( rc ) );
		return rc;
	}

	/* Configure interrupt zero to write to dummy location */
	pci_msix_map ( &intelxl->msix, 0, virt_to_bus ( &intelxl->msg ), 0 );

	/* Enable dummy interrupt zero */
	pci_msix_unmask ( &intelxl->msix, 0 );

	return 0;
}

/**
 * Disable MSI-X dummy interrupt
 *
 * @v intelxl		Intel device
 * @v pci		PCI device
 */
void intelxl_msix_disable ( struct intelxl_nic *intelxl,
			    struct pci_device *pci ) {

	/* Disable dummy interrupt zero */
	pci_msix_mask ( &intelxl->msix, 0 );

	/* Disable MSI-X capability */
	pci_msix_disable ( pci, &intelxl->msix );
}

/******************************************************************************
 *
 * Admin queue
 *
 ******************************************************************************
 */

/** Admin queue register offsets */
static const struct intelxl_admin_offsets intelxl_admin_offsets = {
	.bal = INTELXL_ADMIN_BAL,
	.bah = INTELXL_ADMIN_BAH,
	.len = INTELXL_ADMIN_LEN,
	.head = INTELXL_ADMIN_HEAD,
	.tail = INTELXL_ADMIN_TAIL,
};

/**
 * Allocate admin queue
 *
 * @v intelxl		Intel device
 * @v admin		Admin queue
 * @ret rc		Return status code
 */
static int intelxl_alloc_admin ( struct intelxl_nic *intelxl,
				 struct intelxl_admin *admin ) {
	size_t buf_len = ( sizeof ( admin->buf[0] ) * INTELXL_ADMIN_NUM_DESC );
	size_t len = ( sizeof ( admin->desc[0] ) * INTELXL_ADMIN_NUM_DESC );

	/* Allocate admin queue */
	admin->buf = malloc_dma ( ( buf_len + len ), INTELXL_ALIGN );
	if ( ! admin->buf )
		return -ENOMEM;
	admin->desc = ( ( ( void * ) admin->buf ) + buf_len );

	DBGC ( intelxl, "INTELXL %p A%cQ is at [%08llx,%08llx) buf "
	       "[%08llx,%08llx)\n", intelxl,
	       ( ( admin == &intelxl->command ) ? 'T' : 'R' ),
	       ( ( unsigned long long ) virt_to_bus ( admin->desc ) ),
	       ( ( unsigned long long ) ( virt_to_bus ( admin->desc ) + len ) ),
	       ( ( unsigned long long ) virt_to_bus ( admin->buf ) ),
	       ( ( unsigned long long ) ( virt_to_bus ( admin->buf ) +
					  buf_len ) ) );
	return 0;
}

/**
 * Enable admin queue
 *
 * @v intelxl		Intel device
 * @v admin		Admin queue
 */
static void intelxl_enable_admin ( struct intelxl_nic *intelxl,
				   struct intelxl_admin *admin ) {
	size_t len = ( sizeof ( admin->desc[0] ) * INTELXL_ADMIN_NUM_DESC );
	const struct intelxl_admin_offsets *regs = admin->regs;
	void *admin_regs = ( intelxl->regs + admin->base );
	physaddr_t address;

	/* Initialise admin queue */
	memset ( admin->desc, 0, len );

	/* Reset head and tail registers */
	writel ( 0, admin_regs + regs->head );
	writel ( 0, admin_regs + regs->tail );

	/* Reset queue index */
	admin->index = 0;

	/* Program queue address */
	address = virt_to_bus ( admin->desc );
	writel ( ( address & 0xffffffffUL ), admin_regs + regs->bal );
	if ( sizeof ( physaddr_t ) > sizeof ( uint32_t ) ) {
		writel ( ( ( ( uint64_t ) address ) >> 32 ),
			 admin_regs + regs->bah );
	} else {
		writel ( 0, admin_regs + regs->bah );
	}

	/* Program queue length and enable queue */
	writel ( ( INTELXL_ADMIN_LEN_LEN ( INTELXL_ADMIN_NUM_DESC ) |
		   INTELXL_ADMIN_LEN_ENABLE ),
		 admin_regs + regs->len );
}

/**
 * Disable admin queue
 *
 * @v intelxl		Intel device
 * @v admin		Admin queue
 */
static void intelxl_disable_admin ( struct intelxl_nic *intelxl,
				    struct intelxl_admin *admin ) {
	const struct intelxl_admin_offsets *regs = admin->regs;
	void *admin_regs = ( intelxl->regs + admin->base );

	/* Disable queue */
	writel ( 0, admin_regs + regs->len );
}

/**
 * Free admin queue
 *
 * @v intelxl		Intel device
 * @v admin		Admin queue
 */
static void intelxl_free_admin ( struct intelxl_nic *intelxl __unused,
				 struct intelxl_admin *admin ) {
	size_t buf_len = ( sizeof ( admin->buf[0] ) * INTELXL_ADMIN_NUM_DESC );
	size_t len = ( sizeof ( admin->desc[0] ) * INTELXL_ADMIN_NUM_DESC );

	/* Free queue */
	free_dma ( admin->buf, ( buf_len + len ) );
}

/**
 * Get next admin command queue descriptor
 *
 * @v intelxl		Intel device
 * @ret cmd		Command descriptor
 */
struct intelxl_admin_descriptor *
intelxl_admin_command_descriptor ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin *admin = &intelxl->command;
	struct intelxl_admin_descriptor *cmd;

	/* Get and initialise next descriptor */
	cmd = &admin->desc[ admin->index % INTELXL_ADMIN_NUM_DESC ];
	memset ( cmd, 0, sizeof ( *cmd ) );
	return cmd;
}

/**
 * Get next admin command queue data buffer
 *
 * @v intelxl		Intel device
 * @ret buf		Data buffer
 */
union intelxl_admin_buffer *
intelxl_admin_command_buffer ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin *admin = &intelxl->command;
	union intelxl_admin_buffer *buf;

	/* Get next data buffer */
	buf = &admin->buf[ admin->index % INTELXL_ADMIN_NUM_DESC ];
	memset ( buf, 0, sizeof ( *buf ) );
	return buf;
}

/**
 * Initialise admin event queue descriptor
 *
 * @v intelxl		Intel device
 * @v index		Event queue index
 */
static void intelxl_admin_event_init ( struct intelxl_nic *intelxl,
				       unsigned int index ) {
	struct intelxl_admin *admin = &intelxl->event;
	struct intelxl_admin_descriptor *evt;
	union intelxl_admin_buffer *buf;
	uint64_t address;

	/* Initialise descriptor */
	evt = &admin->desc[ index % INTELXL_ADMIN_NUM_DESC ];
	buf = &admin->buf[ index % INTELXL_ADMIN_NUM_DESC ];
	address = virt_to_bus ( buf );
	evt->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_BUF );
	evt->len = cpu_to_le16 ( sizeof ( *buf ) );
	evt->params.buffer.high = cpu_to_le32 ( address >> 32 );
	evt->params.buffer.low = cpu_to_le32 ( address & 0xffffffffUL );
}

/**
 * Issue admin queue command
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
int intelxl_admin_command ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin *admin = &intelxl->command;
	const struct intelxl_admin_offsets *regs = admin->regs;
	void *admin_regs = ( intelxl->regs + admin->base );
	struct intelxl_admin_descriptor *cmd;
	union intelxl_admin_buffer *buf;
	uint64_t address;
	uint32_t cookie;
	unsigned int index;
	unsigned int tail;
	unsigned int i;
	int rc;

	/* Get next queue entry */
	index = admin->index++;
	tail = ( admin->index % INTELXL_ADMIN_NUM_DESC );
	cmd = &admin->desc[ index % INTELXL_ADMIN_NUM_DESC ];
	buf = &admin->buf[ index % INTELXL_ADMIN_NUM_DESC ];
	DBGC2 ( intelxl, "INTELXL %p admin command %#x opcode %#04x",
		intelxl, index, le16_to_cpu ( cmd->opcode ) );
	if ( cmd->vopcode )
		DBGC2 ( intelxl, "/%#08x", le32_to_cpu ( cmd->vopcode ) );
	DBGC2 ( intelxl, ":\n" );

	/* Sanity checks */
	assert ( ! ( cmd->flags & cpu_to_le16 ( INTELXL_ADMIN_FL_DD ) ) );
	assert ( ! ( cmd->flags & cpu_to_le16 ( INTELXL_ADMIN_FL_CMP ) ) );
	assert ( ! ( cmd->flags & cpu_to_le16 ( INTELXL_ADMIN_FL_ERR ) ) );
	assert ( cmd->ret == 0 );

	/* Populate data buffer address if applicable */
	if ( cmd->flags & cpu_to_le16 ( INTELXL_ADMIN_FL_BUF ) ) {
		address = virt_to_bus ( buf );
		cmd->params.buffer.high = cpu_to_le32 ( address >> 32 );
		cmd->params.buffer.low = cpu_to_le32 ( address & 0xffffffffUL );
	}

	/* Populate cookie, if not being (ab)used for VF opcode */
	if ( ! cmd->vopcode )
		cmd->cookie = cpu_to_le32 ( index );

	/* Record cookie */
	cookie = cmd->cookie;

	/* Post command descriptor */
	DBGC2_HDA ( intelxl, virt_to_phys ( cmd ), cmd, sizeof ( *cmd ) );
	if ( cmd->flags & cpu_to_le16 ( INTELXL_ADMIN_FL_BUF ) ) {
		DBGC2_HDA ( intelxl, virt_to_phys ( buf ), buf,
			    le16_to_cpu ( cmd->len ) );
	}
	wmb();
	writel ( tail, admin_regs + regs->tail );

	/* Wait for completion */
	for ( i = 0 ; i < INTELXL_ADMIN_MAX_WAIT_MS ; i++ ) {

		/* If response is not complete, delay 1ms and retry */
		if ( ! ( cmd->flags & INTELXL_ADMIN_FL_DD ) ) {
			mdelay ( 1 );
			continue;
		}
		DBGC2 ( intelxl, "INTELXL %p admin command %#x response:\n",
			intelxl, index );
		DBGC2_HDA ( intelxl, virt_to_phys ( cmd ), cmd,
			    sizeof ( *cmd ) );

		/* Check for cookie mismatch */
		if ( cmd->cookie != cookie ) {
			DBGC ( intelxl, "INTELXL %p admin command %#x bad "
			       "cookie %#x\n", intelxl, index,
			       le32_to_cpu ( cmd->cookie ) );
			rc = -EPROTO;
			goto err;
		}

		/* Check for errors */
		if ( cmd->ret != 0 ) {
			DBGC ( intelxl, "INTELXL %p admin command %#x error "
			       "%d\n", intelxl, index,
			       le16_to_cpu ( cmd->ret ) );
			rc = -EIO;
			goto err;
		}

		/* Success */
		return 0;
	}

	rc = -ETIMEDOUT;
	DBGC ( intelxl, "INTELXL %p timed out waiting for admin command %#x:\n",
	       intelxl, index );
 err:
	DBGC_HDA ( intelxl, virt_to_phys ( cmd ), cmd, sizeof ( *cmd ) );
	return rc;
}

/**
 * Get firmware version
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int intelxl_admin_version ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin_descriptor *cmd;
	struct intelxl_admin_version_params *version;
	unsigned int api;
	int rc;

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_VERSION );
	version = &cmd->params.version;

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
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
	struct intelxl_admin_descriptor *cmd;
	struct intelxl_admin_driver_params *driver;
	union intelxl_admin_buffer *buf;
	int rc;

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_DRIVER );
	cmd->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_RD | INTELXL_ADMIN_FL_BUF );
	cmd->len = cpu_to_le16 ( sizeof ( buf->driver ) );
	driver = &cmd->params.driver;
	driver->major = product_major_version;
	driver->minor = product_minor_version;
	buf = intelxl_admin_command_buffer ( intelxl );
	snprintf ( buf->driver.name, sizeof ( buf->driver.name ), "%s",
		   ( product_name[0] ? product_name : product_short_name ) );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
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
	struct intelxl_admin_descriptor *cmd;
	struct intelxl_admin_shutdown_params *shutdown;
	int rc;

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_SHUTDOWN );
	shutdown = &cmd->params.shutdown;
	shutdown->unloading = INTELXL_ADMIN_SHUTDOWN_UNLOADING;

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Clear PXE mode
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int intelxl_admin_clear_pxe ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin_descriptor *cmd;
	struct intelxl_admin_clear_pxe_params *pxe;
	uint32_t gllan_rctl_0;
	int rc;

	/* Do nothing if device is already out of PXE mode */
	gllan_rctl_0 = readl ( intelxl->regs + INTELXL_GLLAN_RCTL_0 );
	if ( ! ( gllan_rctl_0 & INTELXL_GLLAN_RCTL_0_PXE_MODE ) ) {
		DBGC2 ( intelxl, "INTELXL %p already in non-PXE mode\n",
			intelxl );
		return 0;
	}

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_CLEAR_PXE );
	pxe = &cmd->params.pxe;
	pxe->magic = INTELXL_ADMIN_CLEAR_PXE_MAGIC;

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
static int intelxl_admin_switch ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin_descriptor *cmd;
	struct intelxl_admin_switch_params *sw;
	union intelxl_admin_buffer *buf;
	int rc;

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_SWITCH );
	cmd->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_BUF );
	cmd->len = cpu_to_le16 ( sizeof ( buf->sw ) );
	sw = &cmd->params.sw;
	buf = intelxl_admin_command_buffer ( intelxl );

	/* Get each configuration in turn */
	do {
		/* Issue command */
		if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
			return rc;

		/* Dump raw configuration */
		DBGC2 ( intelxl, "INTELXL %p SEID %#04x:\n",
			intelxl, le16_to_cpu ( buf->sw.cfg.seid ) );
		DBGC2_HDA ( intelxl, 0, &buf->sw.cfg, sizeof ( buf->sw.cfg ) );

		/* Parse response */
		if ( buf->sw.cfg.type == INTELXL_ADMIN_SWITCH_TYPE_VSI ) {
			intelxl->vsi = le16_to_cpu ( buf->sw.cfg.seid );
			DBGC ( intelxl, "INTELXL %p VSI %#04x uplink %#04x "
			       "downlink %#04x conn %#02x\n", intelxl,
			       intelxl->vsi, le16_to_cpu ( buf->sw.cfg.uplink ),
			       le16_to_cpu ( buf->sw.cfg.downlink ),
			       buf->sw.cfg.connection );
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
	struct intelxl_admin_descriptor *cmd;
	struct intelxl_admin_vsi_params *vsi;
	union intelxl_admin_buffer *buf;
	int rc;

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_VSI );
	cmd->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_BUF );
	cmd->len = cpu_to_le16 ( sizeof ( buf->vsi ) );
	vsi = &cmd->params.vsi;
	vsi->vsi = cpu_to_le16 ( intelxl->vsi );
	buf = intelxl_admin_command_buffer ( intelxl );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
		return rc;

	/* Parse response */
	intelxl->queue = le16_to_cpu ( buf->vsi.queue[0] );
	intelxl->qset = le16_to_cpu ( buf->vsi.qset[0] );
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
	struct intelxl_admin_descriptor *cmd;
	struct intelxl_admin_promisc_params *promisc;
	uint16_t flags;
	int rc;

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_PROMISC );
	flags = ( INTELXL_ADMIN_PROMISC_FL_UNICAST |
		  INTELXL_ADMIN_PROMISC_FL_MULTICAST |
		  INTELXL_ADMIN_PROMISC_FL_BROADCAST |
		  INTELXL_ADMIN_PROMISC_FL_VLAN );
	promisc = &cmd->params.promisc;
	promisc->flags = cpu_to_le16 ( flags );
	promisc->valid = cpu_to_le16 ( flags );
	promisc->vsi = cpu_to_le16 ( intelxl->vsi );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
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
	struct intelxl_admin_descriptor *cmd;
	struct intelxl_admin_autoneg_params *autoneg;
	int rc;

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
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
static int intelxl_admin_link ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct intelxl_admin_descriptor *cmd;
	struct intelxl_admin_link_params *link;
	int rc;

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_LINK );
	link = &cmd->params.link;
	link->notify = INTELXL_ADMIN_LINK_NOTIFY;

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
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
 * Handle virtual function event (when VF driver is not present)
 *
 * @v netdev		Network device
 * @v evt		Admin queue event descriptor
 * @v buf		Admin queue event data buffer
 */
__weak void
intelxlvf_admin_event ( struct net_device *netdev __unused,
			struct intelxl_admin_descriptor *evt __unused,
			union intelxl_admin_buffer *buf __unused ) {

	/* Nothing to do */
}

/**
 * Refill admin event queue
 *
 * @v intelxl		Intel device
 */
static void intelxl_refill_admin ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin *admin = &intelxl->event;
	const struct intelxl_admin_offsets *regs = admin->regs;
	void *admin_regs = ( intelxl->regs + admin->base );
	unsigned int tail;

	/* Update tail pointer */
	tail = ( ( admin->index + INTELXL_ADMIN_NUM_DESC - 1 ) %
		 INTELXL_ADMIN_NUM_DESC );
	wmb();
	writel ( tail, admin_regs + regs->tail );
}

/**
 * Poll admin event queue
 *
 * @v netdev		Network device
 */
void intelxl_poll_admin ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct intelxl_admin *admin = &intelxl->event;
	struct intelxl_admin_descriptor *evt;
	union intelxl_admin_buffer *buf;

	/* Check for events */
	while ( 1 ) {

		/* Get next event descriptor and data buffer */
		evt = &admin->desc[ admin->index % INTELXL_ADMIN_NUM_DESC ];
		buf = &admin->buf[ admin->index % INTELXL_ADMIN_NUM_DESC ];

		/* Stop if descriptor is not yet completed */
		if ( ! ( evt->flags & INTELXL_ADMIN_FL_DD ) )
			return;
		DBGC2 ( intelxl, "INTELXL %p admin event %#x:\n",
			intelxl, admin->index );
		DBGC2_HDA ( intelxl, virt_to_phys ( evt ), evt,
			    sizeof ( *evt ) );
		if ( evt->flags & cpu_to_le16 ( INTELXL_ADMIN_FL_BUF ) ) {
			DBGC2_HDA ( intelxl, virt_to_phys ( buf ), buf,
				    le16_to_cpu ( evt->len ) );
		}

		/* Handle event */
		switch ( evt->opcode ) {
		case cpu_to_le16 ( INTELXL_ADMIN_LINK ):
			intelxl_admin_link ( netdev );
			break;
		case cpu_to_le16 ( INTELXL_ADMIN_SEND_TO_VF ):
			intelxlvf_admin_event ( netdev, evt, buf );
			break;
		default:
			DBGC ( intelxl, "INTELXL %p admin event %#x "
			       "unrecognised opcode %#04x\n", intelxl,
			       admin->index, le16_to_cpu ( evt->opcode ) );
			break;
		}

		/* Reset descriptor and refill queue */
		intelxl_admin_event_init ( intelxl, admin->index );
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
int intelxl_open_admin ( struct intelxl_nic *intelxl ) {
	int rc;

	/* Allocate admin event queue */
	if ( ( rc = intelxl_alloc_admin ( intelxl, &intelxl->event ) ) != 0 )
		goto err_alloc_event;

	/* Allocate admin command queue */
	if ( ( rc = intelxl_alloc_admin ( intelxl, &intelxl->command ) ) != 0 )
		goto err_alloc_command;

	/* (Re)open admin queues */
	intelxl_reopen_admin ( intelxl );

	/* Get firmware version */
	if ( ( rc = intelxl_admin_version ( intelxl ) ) != 0 )
		goto err_version;

	/* Report driver version */
	if ( ( rc = intelxl_admin_driver ( intelxl ) ) != 0 )
		goto err_driver;

	return 0;

 err_driver:
 err_version:
	intelxl_disable_admin ( intelxl, &intelxl->command );
	intelxl_disable_admin ( intelxl, &intelxl->event );
	intelxl_free_admin ( intelxl, &intelxl->command );
 err_alloc_command:
	intelxl_free_admin ( intelxl, &intelxl->event );
 err_alloc_event:
	return rc;
}

/**
 * Reopen admin queues (after virtual function reset)
 *
 * @v intelxl		Intel device
 */
void intelxl_reopen_admin ( struct intelxl_nic *intelxl ) {
	unsigned int i;

	/* Enable admin event queue */
	intelxl_enable_admin ( intelxl, &intelxl->event );

	/* Enable admin command queue */
	intelxl_enable_admin ( intelxl, &intelxl->command );

	/* Initialise all admin event queue descriptors */
	for ( i = 0 ; i < INTELXL_ADMIN_NUM_DESC ; i++ )
		intelxl_admin_event_init ( intelxl, i );

	/* Post all descriptors to event queue */
	intelxl_refill_admin ( intelxl );
}

/**
 * Close admin queues
 *
 * @v intelxl		Intel device
 */
void intelxl_close_admin ( struct intelxl_nic *intelxl ) {

	/* Shut down admin queues */
	intelxl_admin_shutdown ( intelxl );

	/* Disable admin queues */
	intelxl_disable_admin ( intelxl, &intelxl->command );
	intelxl_disable_admin ( intelxl, &intelxl->event );

	/* Free admin queues */
	intelxl_free_admin ( intelxl, &intelxl->command );
	intelxl_free_admin ( intelxl, &intelxl->event );
}

/******************************************************************************
 *
 * Descriptor rings
 *
 ******************************************************************************
 */

/**
 * Allocate descriptor ring
 *
 * @v intelxl		Intel device
 * @v ring		Descriptor ring
 * @ret rc		Return status code
 */
int intelxl_alloc_ring ( struct intelxl_nic *intelxl,
			 struct intelxl_ring *ring ) {
	physaddr_t address;
	int rc;

	/* Allocate descriptor ring */
	ring->desc.raw = malloc_dma ( ring->len, INTELXL_ALIGN );
	if ( ! ring->desc.raw ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	address = virt_to_bus ( ring->desc.raw );

	/* Initialise descriptor ring */
	memset ( ring->desc.raw, 0, ring->len );

	/* Reset tail pointer */
	writel ( 0, ( intelxl->regs + ring->tail ) );

	/* Reset counters */
	ring->prod = 0;
	ring->cons = 0;

	DBGC ( intelxl, "INTELXL %p ring %06x is at [%08llx,%08llx)\n",
	       intelxl, ( ring->reg + ring->tail ),
	       ( ( unsigned long long ) address ),
	       ( ( unsigned long long ) address + ring->len ) );

	return 0;

	free_dma ( ring->desc.raw, ring->len );
 err_alloc:
	return rc;
}

/**
 * Free descriptor ring
 *
 * @v intelxl		Intel device
 * @v ring		Descriptor ring
 */
void intelxl_free_ring ( struct intelxl_nic *intelxl __unused,
			 struct intelxl_ring *ring ) {

	/* Free descriptor ring */
	free_dma ( ring->desc.raw, ring->len );
	ring->desc.raw = NULL;
}

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
	ctx.rx.flags = ( INTELXL_CTX_RX_FL_DSIZE | INTELXL_CTX_RX_FL_CRCSTRIP );
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
	physaddr_t address;
	int rc;

	/* Allocate descriptor ring */
	if ( ( rc = intelxl_alloc_ring ( intelxl, ring ) ) != 0 )
		goto err_alloc;

	/* Program queue context */
	address = virt_to_bus ( ring->desc.raw );
	if ( ( rc = ring->context ( intelxl, address ) ) != 0 )
		goto err_context;

	/* Enable ring */
	if ( ( rc = intelxl_enable_ring ( intelxl, ring ) ) != 0 )
		goto err_enable;

	return 0;

	intelxl_disable_ring ( intelxl, ring );
 err_enable:
 err_context:
	intelxl_free_ring ( intelxl, ring );
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
	intelxl_free_ring ( intelxl, ring );
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
		rx = &intelxl->rx.desc.rx[rx_idx].data;

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
		writel ( rx_tail, ( intelxl->regs + intelxl->rx.tail ) );
	}
}

/**
 * Discard unused receive I/O buffers
 *
 * @v intelxl		Intel device
 */
void intelxl_empty_rx ( struct intelxl_nic *intelxl ) {
	unsigned int i;

	/* Discard any unused receive buffers */
	for ( i = 0 ; i < INTELXL_RX_NUM_DESC ; i++ ) {
		if ( intelxl->rx_iobuf[i] )
			free_iob ( intelxl->rx_iobuf[i] );
		intelxl->rx_iobuf[i] = NULL;
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
	intelxl_empty_rx ( intelxl );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
int intelxl_transmit ( struct net_device *netdev, struct io_buffer *iobuf ) {
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
	tx = &intelxl->tx.desc.tx[tx_idx].data;

	/* Populate transmit descriptor */
	address = virt_to_bus ( iobuf->data );
	len = iob_len ( iobuf );
	tx->address = cpu_to_le64 ( address );
	tx->len = cpu_to_le32 ( INTELXL_TX_DATA_LEN ( len ) );
	tx->flags = cpu_to_le32 ( INTELXL_TX_DATA_DTYP | INTELXL_TX_DATA_EOP |
				  INTELXL_TX_DATA_RS | INTELXL_TX_DATA_JFDI );
	wmb();

	/* Notify card that there are packets ready to transmit */
	writel ( tx_tail, ( intelxl->regs + intelxl->tx.tail ) );

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
		tx_wb = &intelxl->tx.desc.tx[tx_idx].wb;

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
	unsigned int tag;
	size_t len;

	/* Check for received packets */
	while ( intelxl->rx.cons != intelxl->rx.prod ) {

		/* Get next receive descriptor */
		rx_idx = ( intelxl->rx.cons % INTELXL_RX_NUM_DESC );
		rx_wb = &intelxl->rx.desc.rx[rx_idx].wb;

		/* Stop if descriptor is still in use */
		if ( ! ( rx_wb->flags & cpu_to_le32 ( INTELXL_RX_WB_FL_DD ) ) )
			return;

		/* Populate I/O buffer */
		iobuf = intelxl->rx_iobuf[rx_idx];
		intelxl->rx_iobuf[rx_idx] = NULL;
		len = INTELXL_RX_WB_LEN ( le32_to_cpu ( rx_wb->len ) );
		iob_put ( iobuf, len );

		/* Find VLAN device, if applicable */
		if ( rx_wb->flags & cpu_to_le32 ( INTELXL_RX_WB_FL_VLAN ) ) {
			tag = VLAN_TAG ( le16_to_cpu ( rx_wb->vlan ) );
		} else {
			tag = 0;
		}

		/* Hand off to network stack */
		if ( rx_wb->flags & cpu_to_le32 ( INTELXL_RX_WB_FL_RXE ) ) {
			DBGC ( intelxl, "INTELXL %p RX %d error (length %zd, "
			       "flags %08x)\n", intelxl, rx_idx, len,
			       le32_to_cpu ( rx_wb->flags ) );
			vlan_netdev_rx_err ( netdev, tag, iobuf, -EIO );
		} else {
			DBGC2 ( intelxl, "INTELXL %p RX %d complete (length "
				"%zd)\n", intelxl, rx_idx, len );
			vlan_netdev_rx ( netdev, tag, iobuf );
		}
		intelxl->rx.cons++;
	}
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
void intelxl_poll ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;

	/* Poll for completed packets */
	intelxl_poll_tx ( netdev );

	/* Poll for received packets */
	intelxl_poll_rx ( netdev );

	/* Poll for admin events */
	intelxl_poll_admin ( netdev );

	/* Refill RX ring */
	intelxl_refill_rx ( intelxl );

	/* Rearm interrupt, since otherwise receive descriptors will
	 * be written back only after a complete cacheline (four
	 * packets) have been received.
	 *
	 * There is unfortunately no efficient way to determine
	 * whether or not rearming the interrupt is necessary.  If we
	 * are running inside a hypervisor (e.g. using a VF or PF as a
	 * passed-through PCI device), then the MSI-X write is
	 * redirected by the hypervisor to the real host APIC and the
	 * host ISR then raises an interrupt within the guest.  We
	 * therefore cannot poll the nominal MSI-X target location to
	 * watch for the value being written.  We could read from the
	 * INT_DYN_CTL register, but this is even less efficient than
	 * just unconditionally rearming the interrupt.
	 */
	writel ( INTELXL_INT_DYN_CTL_INTENA, intelxl->regs + intelxl->intr );
}

/** Network device operations */
static struct net_device_operations intelxl_operations = {
	.open		= intelxl_open,
	.close		= intelxl_close,
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
	intelxl->intr = INTELXL_PFINT_DYN_CTL0;
	intelxl_init_admin ( &intelxl->command, INTELXL_ADMIN_CMD,
			     &intelxl_admin_offsets );
	intelxl_init_admin ( &intelxl->event, INTELXL_ADMIN_EVT,
			     &intelxl_admin_offsets );
	intelxl_init_ring ( &intelxl->tx, INTELXL_TX_NUM_DESC,
			    sizeof ( intelxl->tx.desc.tx[0] ),
			    intelxl_context_tx );
	intelxl_init_ring ( &intelxl->rx, INTELXL_RX_NUM_DESC,
			    sizeof ( intelxl->rx.desc.rx[0] ),
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

	/* Enable MSI-X dummy interrupt */
	if ( ( rc = intelxl_msix_enable ( intelxl, pci ) ) != 0 )
		goto err_msix;

	/* Open admin queues */
	if ( ( rc = intelxl_open_admin ( intelxl ) ) != 0 )
		goto err_open_admin;

	/* Clear PXE mode */
	if ( ( rc = intelxl_admin_clear_pxe ( intelxl ) ) != 0 )
		goto err_admin_clear_pxe;

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
	intelxl->tx.tail = ( intelxl->tx.reg + INTELXL_QXX_TAIL );
	intelxl->rx.reg = INTELXL_QRX ( intelxl->queue );
	intelxl->rx.tail = ( intelxl->rx.reg + INTELXL_QXX_TAIL );

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
 err_admin_clear_pxe:
	intelxl_close_admin ( intelxl );
 err_open_admin:
	intelxl_msix_disable ( intelxl, pci );
 err_msix:
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

	/* Disable MSI-X dummy interrupt */
	intelxl_msix_disable ( intelxl, pci );

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
