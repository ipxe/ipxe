/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
Prism2 NIC driver for Etherboot
Wrapper for prism2_pci

Written by Michael Brown of Fen Systems Ltd
$Id$
***************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#define WLAN_HOSTIF WLAN_PCI
#include "prism2.c"

static struct pci_id prism2_pci_nics[] = {
PCI_ROM(0x1260, 0x3873, "prism2_pci", "Harris Semiconductor Prism2.5 clone"),	/* Generic Prism2.5 PCI device */
};

static struct pci_driver prism2_pci_driver __pci_driver = {
	.type     = NIC_DRIVER,
	.name     = "Prism2_PCI",
	.probe    = prism2_pci_probe,
	.ids      = prism2_pci_nics,
	.id_count = sizeof(prism2_pci_nics)/sizeof(prism2_pci_nics[0]),
	.class    = 0,
};

