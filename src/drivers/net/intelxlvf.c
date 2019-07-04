/*
 * Copyright (C) 2019 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/pci.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include "intelxlvf.h"

/** @file
 *
 * Intel 40 Gigabit Ethernet virtual function network card driver
 *
 */

/******************************************************************************
 *
 * Device reset
 *
 ******************************************************************************
 */

/**
 * Reset hardware via PCIe function-level reset
 *
 * @v intelxl		Intel device
 */
static void intelxlvf_reset_flr ( struct intelxl_nic *intelxl,
				  struct pci_device *pci ) {
	uint16_t control;

	/* Perform a PCIe function-level reset */
	pci_read_config_word ( pci, ( intelxl->exp + PCI_EXP_DEVCTL ),
			       &control );
	pci_write_config_word ( pci, ( intelxl->exp + PCI_EXP_DEVCTL ),
				( control | PCI_EXP_DEVCTL_FLR ) );
	mdelay ( INTELXL_RESET_DELAY_MS );
}

/**
 * Wait for admin event queue to be torn down
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int intelxlvf_reset_wait_teardown ( struct intelxl_nic *intelxl ) {
	uint32_t admin_evt_len;
	unsigned int i;

	/* Wait for admin event queue to be torn down */
	for ( i = 0 ; i < INTELXLVF_RESET_MAX_WAIT_MS ; i++ ) {

		/* Check admin event queue length register */
		admin_evt_len = readl ( intelxl->regs + INTELXLVF_ADMIN +
					INTELXLVF_ADMIN_EVT_LEN );
		if ( ! ( admin_evt_len & INTELXL_ADMIN_LEN_ENABLE ) )
			return 0;

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( intelxl, "INTELXL %p timed out waiting for teardown (%#08x)\n",
	       intelxl, admin_evt_len );
	return -ETIMEDOUT;
}

/**
 * Wait for virtual function to be marked as active
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int intelxlvf_reset_wait_active ( struct intelxl_nic *intelxl ) {
	uint32_t vfgen_rstat;
	unsigned int vfr_state;
	unsigned int i;

	/* Wait for virtual function to be marked as active */
	for ( i = 0 ; i < INTELXLVF_RESET_MAX_WAIT_MS ; i++ ) {

		/* Check status as written by physical function driver */
		vfgen_rstat = readl ( intelxl->regs + INTELXLVF_VFGEN_RSTAT );
		vfr_state = INTELXLVF_VFGEN_RSTAT_VFR_STATE ( vfgen_rstat );
		if ( vfr_state == INTELXLVF_VFGEN_RSTAT_VFR_STATE_ACTIVE )
			return 0;

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( intelxl, "INTELXL %p timed out waiting for activation "
	       "(%#08x)\n", intelxl, vfgen_rstat );
	return -ETIMEDOUT;
}

/**
 * Reset hardware via admin queue
 *
 * @v intelxl		Intel device
 * @ret rc		Return status code
 */
static int intelxlvf_reset_admin ( struct intelxl_nic *intelxl ) {
	struct intelxl_admin_descriptor *cmd;
	int rc;

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_SEND_TO_PF );
	cmd->vopcode = cpu_to_le32 ( INTELXL_ADMIN_VF_RESET );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
		goto err_command;

	/* Wait for minimum reset time */
	mdelay ( INTELXL_RESET_DELAY_MS );

	/* Wait for reset to take effect */
	if ( ( rc = intelxlvf_reset_wait_teardown ( intelxl ) ) != 0 )
		goto err_teardown;

	/* Wait for virtual function to become active */
	if ( ( rc = intelxlvf_reset_wait_active ( intelxl ) ) != 0 )
		goto err_active;

 err_active:
 err_teardown:
	intelxl_reopen_admin ( intelxl );
 err_command:
	return rc;
}

/******************************************************************************
 *
 * Admin queue
 *
 ******************************************************************************
 */

/** Admin command queue register offsets */
static const struct intelxl_admin_offsets intelxlvf_admin_command_offsets = {
	.bal = INTELXLVF_ADMIN_CMD_BAL,
	.bah = INTELXLVF_ADMIN_CMD_BAH,
	.len = INTELXLVF_ADMIN_CMD_LEN,
	.head = INTELXLVF_ADMIN_CMD_HEAD,
	.tail = INTELXLVF_ADMIN_CMD_TAIL,
};

/** Admin event queue register offsets */
static const struct intelxl_admin_offsets intelxlvf_admin_event_offsets = {
	.bal = INTELXLVF_ADMIN_EVT_BAL,
	.bah = INTELXLVF_ADMIN_EVT_BAH,
	.len = INTELXLVF_ADMIN_EVT_LEN,
	.head = INTELXLVF_ADMIN_EVT_HEAD,
	.tail = INTELXLVF_ADMIN_EVT_TAIL,
};

/**
 * Issue admin queue virtual function command
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int intelxlvf_admin_command ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct intelxl_admin *admin = &intelxl->command;
	struct intelxl_admin_descriptor *cmd;
	unsigned int i;
	int rc;

	/* Populate descriptor */
	cmd = &admin->desc[ admin->index % INTELXL_ADMIN_NUM_DESC ];
	cmd->opcode = cpu_to_le16 ( INTELXL_ADMIN_SEND_TO_PF );

	/* Record opcode */
	intelxl->vopcode = le32_to_cpu ( cmd->vopcode );

	/* Issue command */
	if ( ( rc = intelxl_admin_command ( intelxl ) ) != 0 )
		goto err_command;

	/* Wait for response */
	for ( i = 0 ; i < INTELXLVF_ADMIN_MAX_WAIT_MS ; i++ ) {

		/* Poll admin event queue */
		intelxl_poll_admin ( netdev );

		/* If response has not arrived, delay 1ms and retry */
		if ( intelxl->vopcode ) {
			mdelay ( 1 );
			continue;
		}

		/* Check for errors */
		if ( intelxl->vret != 0 )
			return -EIO;

		return 0;
	}

	rc = -ETIMEDOUT;
	DBGC ( intelxl, "INTELXL %p timed out waiting for admin VF command "
	       "%#x\n", intelxl, intelxl->vopcode );
 err_command:
	intelxl->vopcode = 0;
	return rc;
}

/**
 * Handle link status event
 *
 * @v netdev		Network device
 * @v link		Link status
 */
static void intelxlvf_admin_link ( struct net_device *netdev,
				   struct intelxl_admin_vf_status_link *link ) {
	struct intelxl_nic *intelxl = netdev->priv;

	DBGC ( intelxl, "INTELXL %p link %#02x speed %#02x\n", intelxl,
	       link->status, link->speed );

	/* Update network device */
	if ( link->status ) {
		netdev_link_up ( netdev );
	} else {
		netdev_link_down ( netdev );
	}
}

/**
 * Handle status change event
 *
 * @v netdev		Network device
 * @v stat		Status change event
 */
static void
intelxlvf_admin_status ( struct net_device *netdev,
			 struct intelxl_admin_vf_status_buffer *stat ) {
	struct intelxl_nic *intelxl = netdev->priv;

	/* Handle event */
	switch ( stat->event ) {
	case cpu_to_le32 ( INTELXL_ADMIN_VF_STATUS_LINK ):
		intelxlvf_admin_link ( netdev, &stat->data.link );
		break;
	default:
		DBGC ( intelxl, "INTELXL %p unrecognised status change "
		       "event %#x:\n", intelxl, le32_to_cpu ( stat->event ) );
		DBGC_HDA ( intelxl, 0, stat, sizeof ( *stat ) );
		break;
	}
}

/**
 * Handle virtual function event
 *
 * @v netdev		Network device
 * @v evt		Admin queue event descriptor
 * @v buf		Admin queue event data buffer
 */
void intelxlvf_admin_event ( struct net_device *netdev,
			     struct intelxl_admin_descriptor *evt,
			     union intelxl_admin_buffer *buf ) {
	struct intelxl_nic *intelxl = netdev->priv;
	unsigned int vopcode = le32_to_cpu ( evt->vopcode );

	/* Record command response if applicable */
	if ( vopcode == intelxl->vopcode ) {
		memcpy ( &intelxl->vbuf, buf, sizeof ( intelxl->vbuf ) );
		intelxl->vopcode = 0;
		intelxl->vret = le32_to_cpu ( evt->vret );
		if ( intelxl->vret != 0 ) {
			DBGC ( intelxl, "INTELXL %p admin VF command %#x "
			       "error %d\n", intelxl, vopcode, intelxl->vret );
			DBGC_HDA ( intelxl, virt_to_bus ( evt ), evt,
				   sizeof ( *evt ) );
			DBGC_HDA ( intelxl, virt_to_bus ( buf ), buf,
				   le16_to_cpu ( evt->len ) );
		}
		return;
	}

	/* Handle unsolicited events */
	switch ( vopcode ) {
	case INTELXL_ADMIN_VF_STATUS:
		intelxlvf_admin_status ( netdev, &buf->stat );
		break;
	default:
		DBGC ( intelxl, "INTELXL %p unrecognised VF event %#x:\n",
		       intelxl, vopcode );
		DBGC_HDA ( intelxl, 0, evt, sizeof ( *evt ) );
		DBGC_HDA ( intelxl, 0, buf, le16_to_cpu ( evt->len ) );
		break;
	}
}

/**
 * Get resources
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int intelxlvf_admin_get_resources ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct intelxl_admin_descriptor *cmd;
	struct intelxl_admin_vf_get_resources_buffer *res;
	int rc;

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
	cmd->vopcode = cpu_to_le32 ( INTELXL_ADMIN_VF_GET_RESOURCES );

	/* Issue command */
	if ( ( rc = intelxlvf_admin_command ( netdev ) ) != 0 )
		return rc;

	/* Parse response */
	res = &intelxl->vbuf.res;
	intelxl->vsi = le16_to_cpu ( res->vsi );
	memcpy ( netdev->hw_addr, res->mac, ETH_ALEN );
	DBGC ( intelxl, "INTELXL %p VSI %#04x\n", intelxl, intelxl->vsi );

	return 0;
}

/******************************************************************************
 *
 * Network device interface
 *
 ******************************************************************************
 */

/**
 * Configure queues
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int intelxlvf_admin_configure ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct intelxl_admin_descriptor *cmd;
	union intelxl_admin_buffer *buf;
	int rc;

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
	cmd->vopcode = cpu_to_le32 ( INTELXL_ADMIN_VF_CONFIGURE );
	cmd->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_RD | INTELXL_ADMIN_FL_BUF );
	cmd->len = cpu_to_le16 ( sizeof ( buf->cfg ) );
	buf = intelxl_admin_command_buffer ( intelxl );
	buf->cfg.vsi = cpu_to_le16 ( intelxl->vsi );
	buf->cfg.count = cpu_to_le16 ( 1 );
	buf->cfg.tx.vsi = cpu_to_le16 ( intelxl->vsi );
	buf->cfg.tx.count = cpu_to_le16 ( INTELXL_TX_NUM_DESC );
	buf->cfg.tx.base = cpu_to_le64 ( virt_to_bus ( intelxl->tx.desc.raw ) );
	buf->cfg.rx.vsi = cpu_to_le16 ( intelxl->vsi );
	buf->cfg.rx.count = cpu_to_le32 ( INTELXL_RX_NUM_DESC );
	buf->cfg.rx.len = cpu_to_le32 ( intelxl->mfs );
	buf->cfg.rx.mfs = cpu_to_le32 ( intelxl->mfs );
	buf->cfg.rx.base = cpu_to_le64 ( virt_to_bus ( intelxl->rx.desc.raw ) );

	/* Issue command */
	if ( ( rc = intelxlvf_admin_command ( netdev ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Configure IRQ mapping
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int intelxlvf_admin_irq_map ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct intelxl_admin_descriptor *cmd;
	union intelxl_admin_buffer *buf;
	int rc;

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
	cmd->vopcode = cpu_to_le32 ( INTELXL_ADMIN_VF_IRQ_MAP );
	cmd->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_RD | INTELXL_ADMIN_FL_BUF );
	cmd->len = cpu_to_le16 ( sizeof ( buf->irq ) );
	buf = intelxl_admin_command_buffer ( intelxl );
	buf->irq.count = cpu_to_le16 ( 1 );
	buf->irq.vsi = cpu_to_le16 ( intelxl->vsi );
	buf->irq.rxmap = cpu_to_le16 ( 0x0001 );
	buf->irq.txmap = cpu_to_le16 ( 0x0001 );

	/* Issue command */
	if ( ( rc = intelxlvf_admin_command ( netdev ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Enable/disable queues
 *
 * @v netdev		Network device
 * @v enable		Enable queues
 * @ret rc		Return status code
 */
static int intelxlvf_admin_queues ( struct net_device *netdev, int enable ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct intelxl_admin_descriptor *cmd;
	union intelxl_admin_buffer *buf;
	int rc;

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
	cmd->vopcode = ( enable ? cpu_to_le32 ( INTELXL_ADMIN_VF_ENABLE ) :
			 cpu_to_le32 ( INTELXL_ADMIN_VF_DISABLE ) );
	cmd->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_RD | INTELXL_ADMIN_FL_BUF );
	cmd->len = cpu_to_le16 ( sizeof ( buf->queues ) );
	buf = intelxl_admin_command_buffer ( intelxl );
	buf->queues.vsi = cpu_to_le16 ( intelxl->vsi );
	buf->queues.rx = cpu_to_le32 ( 1 );
	buf->queues.tx = cpu_to_le32 ( 1 );

	/* Issue command */
	if ( ( rc = intelxlvf_admin_command ( netdev ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Configure promiscuous mode
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int intelxlvf_admin_promisc ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	struct intelxl_admin_descriptor *cmd;
	union intelxl_admin_buffer *buf;
	int rc;

	/* Populate descriptor */
	cmd = intelxl_admin_command_descriptor ( intelxl );
	cmd->vopcode = cpu_to_le32 ( INTELXL_ADMIN_VF_PROMISC );
	cmd->flags = cpu_to_le16 ( INTELXL_ADMIN_FL_RD | INTELXL_ADMIN_FL_BUF );
	cmd->len = cpu_to_le16 ( sizeof ( buf->promisc ) );
	buf = intelxl_admin_command_buffer ( intelxl );
	buf->promisc.vsi = cpu_to_le16 ( intelxl->vsi );
	buf->promisc.flags = cpu_to_le16 ( INTELXL_ADMIN_PROMISC_FL_UNICAST |
					   INTELXL_ADMIN_PROMISC_FL_MULTICAST );

	/* Issue command */
	if ( ( rc = intelxlvf_admin_command ( netdev ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int intelxlvf_open ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	int rc;

	/* Calculate maximum frame size */
	intelxl->mfs = ( ( ETH_HLEN + netdev->mtu + 4 /* CRC */ +
			   INTELXL_ALIGN - 1 ) & ~( INTELXL_ALIGN - 1 ) );

	/* Allocate transmit descriptor ring */
	if ( ( rc = intelxl_alloc_ring ( intelxl, &intelxl->tx ) ) != 0 )
		goto err_alloc_tx;

	/* Allocate receive descriptor ring */
	if ( ( rc = intelxl_alloc_ring ( intelxl, &intelxl->rx ) ) != 0 )
		goto err_alloc_rx;

	/* Configure queues */
	if ( ( rc = intelxlvf_admin_configure ( netdev ) ) != 0 )
		goto err_configure;

	/* Configure IRQ map */
	if ( ( rc = intelxlvf_admin_irq_map ( netdev ) ) != 0 )
		goto err_irq_map;

	/* Enable queues */
	if ( ( rc = intelxlvf_admin_queues ( netdev, 1 ) ) != 0 )
		goto err_enable;

	/* Configure promiscuous mode */
	if ( ( rc = intelxlvf_admin_promisc ( netdev ) ) != 0 )
		goto err_promisc;

	return 0;

 err_promisc:
	intelxlvf_admin_queues ( netdev, INTELXL_ADMIN_VF_DISABLE );
 err_enable:
 err_irq_map:
 err_configure:
	intelxl_free_ring ( intelxl, &intelxl->rx );
 err_alloc_rx:
	intelxl_free_ring ( intelxl, &intelxl->tx );
 err_alloc_tx:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void intelxlvf_close ( struct net_device *netdev ) {
	struct intelxl_nic *intelxl = netdev->priv;
	int rc;

	/* Disable queues */
	if ( ( rc = intelxlvf_admin_queues ( netdev, 0 ) ) != 0 ) {
		/* Leak memory; there's nothing else we can do */
		return;
	}

	/* Free receive descriptor ring */
	intelxl_free_ring ( intelxl, &intelxl->rx );

	/* Free transmit descriptor ring */
	intelxl_free_ring ( intelxl, &intelxl->tx );

	/* Discard any unused receive buffers */
	intelxl_empty_rx ( intelxl );
}

/** Network device operations */
static struct net_device_operations intelxlvf_operations = {
	.open		= intelxlvf_open,
	.close		= intelxlvf_close,
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
static int intelxlvf_probe ( struct pci_device *pci ) {
	struct net_device *netdev;
	struct intelxl_nic *intelxl;
	int rc;

	/* Allocate and initialise net device */
	netdev = alloc_etherdev ( sizeof ( *intelxl ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &intelxlvf_operations );
	intelxl = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	netdev->dev = &pci->dev;
	memset ( intelxl, 0, sizeof ( *intelxl ) );
	intelxl->intr = INTELXLVF_VFINT_DYN_CTL0;
	intelxl_init_admin ( &intelxl->command, INTELXLVF_ADMIN,
			     &intelxlvf_admin_command_offsets );
	intelxl_init_admin ( &intelxl->event, INTELXLVF_ADMIN,
			     &intelxlvf_admin_event_offsets );
	intelxlvf_init_ring ( &intelxl->tx, INTELXL_TX_NUM_DESC,
			      sizeof ( intelxl->tx.desc.tx[0] ),
			      INTELXLVF_QTX_TAIL );
	intelxlvf_init_ring ( &intelxl->rx, INTELXL_RX_NUM_DESC,
			      sizeof ( intelxl->rx.desc.rx[0] ),
			      INTELXLVF_QRX_TAIL );

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Map registers */
	intelxl->regs = ioremap ( pci->membase, INTELXLVF_BAR_SIZE );
	if ( ! intelxl->regs ) {
		rc = -ENODEV;
		goto err_ioremap;
	}

	/* Locate PCI Express capability */
	intelxl->exp = pci_find_capability ( pci, PCI_CAP_ID_EXP );
	if ( ! intelxl->exp ) {
		DBGC ( intelxl, "INTELXL %p missing PCIe capability\n",
		       intelxl );
		rc = -ENXIO;
		goto err_exp;
	}

	/* Reset the function via PCIe FLR */
	intelxlvf_reset_flr ( intelxl, pci );

	/* Enable MSI-X dummy interrupt */
	if ( ( rc = intelxl_msix_enable ( intelxl, pci ) ) != 0 )
		goto err_msix;

	/* Open admin queues */
	if ( ( rc = intelxl_open_admin ( intelxl ) ) != 0 )
		goto err_open_admin;

	/* Reset the function via admin queue */
	if ( ( rc = intelxlvf_reset_admin ( intelxl ) ) != 0 )
		goto err_reset_admin;

	/* Get MAC address */
	if ( ( rc = intelxlvf_admin_get_resources ( netdev ) ) != 0 )
		goto err_get_resources;

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	return 0;

	unregister_netdev ( netdev );
 err_register_netdev:
 err_get_resources:
 err_reset_admin:
	intelxl_close_admin ( intelxl );
 err_open_admin:
	intelxl_msix_disable ( intelxl, pci );
 err_msix:
	intelxlvf_reset_flr ( intelxl, pci );
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
static void intelxlvf_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct intelxl_nic *intelxl = netdev->priv;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Reset the function via admin queue */
	intelxlvf_reset_admin ( intelxl );

	/* Close admin queues */
	intelxl_close_admin ( intelxl );

	/* Disable MSI-X dummy interrupt */
	intelxl_msix_disable ( intelxl, pci );

	/* Reset the function via PCIe FLR */
	intelxlvf_reset_flr ( intelxl, pci );

	/* Free network device */
	iounmap ( intelxl->regs );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** PCI device IDs */
static struct pci_device_id intelxlvf_nics[] = {
	PCI_ROM ( 0x8086, 0x154c, "xl710-vf", "XL710 VF", 0 ),
	PCI_ROM ( 0x8086, 0x1571, "xl710-vf-hv", "XL710 VF (Hyper-V)", 0 ),
	PCI_ROM ( 0x8086, 0x1889, "xl710-vf-ad", "XL710 VF (adaptive)", 0 ),
	PCI_ROM ( 0x8086, 0x37cd, "x722-vf", "X722 VF", 0 ),
	PCI_ROM ( 0x8086, 0x37d9, "x722-vf-hv", "X722 VF (Hyper-V)", 0 ),
};

/** PCI driver */
struct pci_driver intelxlvf_driver __pci_driver = {
	.ids = intelxlvf_nics,
	.id_count = ( sizeof ( intelxlvf_nics ) /
		      sizeof ( intelxlvf_nics[0] ) ),
	.probe = intelxlvf_probe,
	.remove = intelxlvf_remove,
};
