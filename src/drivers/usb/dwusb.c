/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ipxe/timer.h>
#include <ipxe/devtree.h>
#include <ipxe/fdt.h>
#include "dwusb.h"

/** @file
 *
 * Synopsys DesignWare USB3 host controller driver
 *
 */

/**
 * Probe devicetree device
 *
 * @v dt		Devicetree device
 * @v offset		Starting node offset
 * @ret rc		Return status code
 */
static int dwusb_probe ( struct dt_device *dt, unsigned int offset ) {
	struct xhci_device *xhci;
	uint32_t gctl;
	int rc;

	/* Allocate and initialise structure */
	xhci = zalloc ( sizeof ( *xhci ) );
	if ( ! xhci ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	xhci->dev = &dt->dev;
	xhci->dma = &dt->dma;

	/* Map registers */
	xhci->regs = dt_ioremap ( dt, offset, 0, 0 );
	if ( ! xhci->regs ) {
		rc = -ENODEV;
		goto err_ioremap;
	}

	/* Reset via global core control register */
	gctl = readl ( xhci->regs + DWUSB_GCTL );
	writel ( ( gctl | DWUSB_GCTL_RESET ), ( xhci->regs + DWUSB_GCTL ) );
	mdelay ( 100 );
	writel ( gctl, ( xhci->regs + DWUSB_GCTL ) );

	/* Configure as a host controller */
	gctl &= ~DWUSB_GCTL_PRTDIR_MASK;
	gctl |= DWUSB_GCTL_PRTDIR_HOST;
	writel ( gctl, ( xhci->regs + DWUSB_GCTL ) );

	/* Initialise xHCI device */
	xhci_init ( xhci );

	/* Register xHCI device */
	if ( ( rc = xhci_register ( xhci ) ) != 0 ) {
		DBGC ( xhci, "XHCI %s could not register: %s\n",
		       xhci->name, strerror ( rc ) );
		goto err_register;
	}

	dt_set_drvdata ( dt, xhci );
	return 0;

	xhci_unregister ( xhci );
 err_register:
	iounmap ( xhci->regs );
 err_ioremap:
	free ( xhci );
 err_alloc:
	return rc;
}

/**
 * Remove devicetree device
 *
 * @v dt		Devicetree device
 */
static void dwusb_remove ( struct dt_device *dt ) {
	struct xhci_device *xhci = dt_get_drvdata ( dt );

	/* Unregister xHCI device */
	xhci_unregister ( xhci );

	/* Unmap registers */
	iounmap ( xhci->regs );

	/* Free device */
	free ( xhci );
}

/** DesignWare USB3 compatible model identifiers */
static const char * dwusb_ids[] = {
	"snps,dwc3",
};

/** DesignWare USB3 devicetree driver */
struct dt_driver dwusb_driver __dt_driver = {
	.name = "dwusb",
	.ids = dwusb_ids,
	.id_count = ( sizeof ( dwusb_ids ) / sizeof ( dwusb_ids[0] ) ),
	.probe = dwusb_probe,
	.remove = dwusb_remove,
};
