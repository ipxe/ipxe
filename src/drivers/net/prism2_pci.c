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

#include <gpxe/pci.h>
#include "nic.h"

#define WLAN_HOSTIF WLAN_PCI
#include "prism2.c"

static int prism2_pci_probe ( struct nic *nic, struct pci_device *pci ) {
  hfa384x_t *hw = &hw_global;
  uint32_t membase = 0; /* Prism2.5 Memory Base */

  pci_fill_nic ( nic, pci );
  pci_read_config_dword( pci, PRISM2_PCI_MEM_BASE, &membase);
  membase &= PCI_BASE_ADDRESS_MEM_MASK;
  hw->membase = (uint32_t) phys_to_virt(membase);
  printf ( "Prism2.5 has registers at %#x\n", hw->membase );
  nic->ioaddr = hw->membase;

  return prism2_probe ( nic, hw );
}

static void prism2_pci_disable ( struct nic *nic,
				 struct pci_device *pci __unused ) {
  prism2_disable ( nic );
}

static struct pci_id prism2_pci_nics[] = {
PCI_ROM(0x1260, 0x3873, "prism2_pci",	"Harris Semiconductor Prism2.5 clone"),
PCI_ROM(0x1260, 0x3873, "hwp01170",	"ActionTec HWP01170"),
PCI_ROM(0x1260, 0x3873, "dwl520",	"DLink DWL-520"),
};

PCI_DRIVER ( prism2_pci_driver, prism2_pci_nics, PCI_NO_CLASS );

DRIVER ( "Prism2/PCI", nic_driver, pci_driver, prism2_pci_driver,
	 prism2_pci_probe, prism2_pci_disable );

