/**************************************************************************
 *
 * Driver datapath common code for Solarflare network cards
 *
 * Written by Shradha Shah <sshah@solarflare.com>
 *
 * Copyright Fen Systems Ltd. 2005
 * Copyright Level 5 Networks Inc. 2005
 * Copyright 2006-2017 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 *
 ***************************************************************************/
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <ipxe/io.h>
#include <ipxe/pci.h>
#include <ipxe/malloc.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>
#include "efx_common.h"
#include "efx_bitfield.h"
#include "mc_driver_pcol.h"

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/*******************************************************************************
 *
 *
 * Low-level hardware access
 *
 *
 ******************************************************************************/

void
efx_writel(struct efx_nic *efx, efx_dword_t *value, unsigned int reg)
{
	DBGCIO(efx, "Writing partial register %x with " EFX_DWORD_FMT "\n",
	       reg, EFX_DWORD_VAL(*value));
	_efx_writel(efx, value->u32[0], reg);
}

void
efx_readl(struct efx_nic *efx, efx_dword_t *value, unsigned int reg)
{
	value->u32[0] = _efx_readl(efx, reg);
	DBGCIO(efx, "Read from register %x, got " EFX_DWORD_FMT "\n",
	       reg, EFX_DWORD_VAL(*value));
}

/*******************************************************************************
 *
 *
 * Inititialization and Close
 *
 *
 ******************************************************************************/
void efx_probe(struct net_device *netdev, enum efx_revision revision)
{
	struct efx_nic *efx = netdev_priv(netdev);
	struct pci_device *pci = container_of(netdev->dev,
					      struct pci_device, dev);

	efx->netdev = netdev;
	efx->revision = revision;

	/* MMIO bar */
	efx->mmio_start = pci_bar_start(pci, PCI_BASE_ADDRESS_2);
	efx->mmio_len = pci_bar_size(pci, PCI_BASE_ADDRESS_2);
	efx->membase = ioremap(efx->mmio_start, efx->mmio_len);

	DBGCP(efx, "BAR of %lx bytes at phys %lx mapped at %p\n",
	      efx->mmio_len, efx->mmio_start, efx->membase);

	/* Enable PCI access */
	adjust_pci_device(pci);
}

void efx_remove(struct net_device *netdev)
{
	struct efx_nic *efx = netdev_priv(netdev);

	iounmap(efx->membase);
	efx->membase = NULL;
}
