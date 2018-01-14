/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <ipxe/malloc.h>
#include <ipxe/pci.h>
#include "skeleton.h"

/** @file
 *
 * Skeleton network driver
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
 * @v skel		Skeleton device
 * @ret rc		Return status code
 */
static int skeleton_reset ( struct skeleton_nic *skel ) {

	DBGC ( skel, "SKELETON %p does not yet support reset\n", skel );
	return -ENOTSUP;
}

/******************************************************************************
 *
 * Link state
 *
 ******************************************************************************
 */

/**
 * Check link state
 *
 * @v netdev		Network device
 */
static void skeleton_check_link ( struct net_device *netdev ) {
	struct skeleton_nic *skel = netdev->priv;

	DBGC ( skel, "SKELETON %p does not yet support link state\n", skel );
	netdev_link_err ( netdev, -ENOTSUP );
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
static int skeleton_open ( struct net_device *netdev ) {
	struct skeleton_nic *skel = netdev->priv;

	DBGC ( skel, "SKELETON %p does not yet support open\n", skel );
	return -ENOTSUP;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void skeleton_close ( struct net_device *netdev ) {
	struct skeleton_nic *skel = netdev->priv;

	DBGC ( skel, "SKELETON %p does not yet support close\n", skel );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int skeleton_transmit ( struct net_device *netdev,
			       struct io_buffer *iobuf ) {
	struct skeleton_nic *skel = netdev->priv;

	DBGC ( skel, "SKELETON %p does not yet support transmit\n", skel );
	( void ) iobuf;
	return -ENOTSUP;
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void skeleton_poll ( struct net_device *netdev ) {
	struct skeleton_nic *skel = netdev->priv;

	/* Not yet implemented */
	( void ) skel;
}

/**
 * Enable or disable interrupts
 *
 * @v netdev		Network device
 * @v enable		Interrupts should be enabled
 */
static void skeleton_irq ( struct net_device *netdev, int enable ) {
	struct skeleton_nic *skel = netdev->priv;

	DBGC ( skel, "SKELETON %p does not yet support interrupts\n", skel );
	( void ) enable;
}

/** Skeleton network device operations */
static struct net_device_operations skeleton_operations = {
	.open		= skeleton_open,
	.close		= skeleton_close,
	.transmit	= skeleton_transmit,
	.poll		= skeleton_poll,
	.irq		= skeleton_irq,
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
static int skeleton_probe ( struct pci_device *pci ) {
	struct net_device *netdev;
	struct skeleton_nic *skel;
	int rc;

	/* Allocate and initialise net device */
	netdev = alloc_etherdev ( sizeof ( *skel ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &skeleton_operations );
	skel = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	netdev->dev = &pci->dev;
	memset ( skel, 0, sizeof ( *skel ) );

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Map registers */
	skel->regs = ioremap ( pci->membase, SKELETON_BAR_SIZE );
	if ( ! skel->regs ) {
		rc = -ENODEV;
		goto err_ioremap;
	}

	/* Reset the NIC */
	if ( ( rc = skeleton_reset ( skel ) ) != 0 )
		goto err_reset;

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	/* Set initial link state */
	skeleton_check_link ( netdev );

	return 0;

	unregister_netdev ( netdev );
 err_register_netdev:
	skeleton_reset ( skel );
 err_reset:
	iounmap ( skel->regs );
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
static void skeleton_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct skeleton_nic *skel = netdev->priv;

	/* Unregister network device */
	unregister_netdev ( netdev );

	/* Reset card */
	skeleton_reset ( skel );

	/* Free network device */
	iounmap ( skel->regs );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** Skeleton PCI device IDs */
static struct pci_device_id skeleton_nics[] = {
	PCI_ROM ( 0x5ce1, 0x5ce1, "skel",	"Skeleton", 0 ),
};

/** Skeleton PCI driver */
struct pci_driver skeleton_driver __pci_driver = {
	.ids = skeleton_nics,
	.id_count = ( sizeof ( skeleton_nics ) / sizeof ( skeleton_nics[0] ) ),
	.probe = skeleton_probe,
	.remove = skeleton_remove,
};
