/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
Prism2 NIC driver for Etherboot
Wrapper for prism2_plx

Written by Michael Brown of Fen Systems Ltd
$Id$
***************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#define WLAN_HOSTIF WLAN_PLX
#include "prism2.c"

static struct pci_id prism2_plx_nics[] = {
PCI_ROM(0x1385, 0x4100, "ma301",         "Netgear MA301"),
PCI_ROM(0x10b7, 0x7770, "3c-airconnect", "3Com AirConnect"),
PCI_ROM(0x111a, 0x1023, "ss1023",        "Siemens SpeedStream SS1023"),
PCI_ROM(0x15e8, 0x0130, "correga",       "Correga"),
PCI_ROM(0x1638, 0x1100, "smc2602w",      "SMC EZConnect SMC2602W"),	/* or Eumitcom PCI WL11000, Addtron AWA-100 */
PCI_ROM(0x16ab, 0x1100, "gl24110p",      "Global Sun Tech GL24110P"),
PCI_ROM(0x16ab, 0x1101, "16ab-1101",     "Unknown"),
PCI_ROM(0x16ab, 0x1102, "wdt11",         "Linksys WDT11"),
PCI_ROM(0x16ec, 0x3685, "usr2415",       "USR 2415"),
PCI_ROM(0xec80, 0xec00, "f5d6000",       "Belkin F5D6000"),
PCI_ROM(0x126c, 0x8030, "emobility",     "Nortel emobility"),
};

static struct pci_driver prism2_plx_driver __pci_driver = {
	.type     = NIC_DRIVER,
	.name     = "Prism2_PLX",
	.probe    = prism2_plx_probe,
	.ids      = prism2_plx_nics,
	.id_count = sizeof(prism2_plx_nics)/sizeof(prism2_plx_nics[0]),
	.class    = 0,
};

