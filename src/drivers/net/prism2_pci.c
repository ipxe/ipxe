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
PCI_ROM(0x1260, 0x3873, "prism2_pci",	"Harris Semiconductor Prism2.5 clone"),
PCI_ROM(0x1260, 0x3873, "hwp01170",	"ActionTec HWP01170"),
PCI_ROM(0x1260, 0x3873, "dwl520",	"DLink DWL-520"),
};

static struct pci_driver prism2_pci_driver =
	PCI_DRIVER ( "Prism2_PCI", prism2_pci_nics, PCI_NO_CLASS );

BOOT_DRIVER ( "Prism2_PCI", prism2_pci_probe );

