#include "etherboot.h"
#include "pci.h"
#include "disk.h"

/*
 *   UBL, The Universal Talkware Boot Loader 
 *    Copyright (C) 2000 Universal Talkware Inc.
 *    Copyright (C) 2002 Eric Biederman
 *   Add to load filo
 *	By LYH  yhlu@tyan.com
 *	
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version. 
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details. 
 * 
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 *
 */

#ifdef CONFIG_PCI
extern int filo(void);

static int filo_pci_probe(struct dev *dev, struct pci_device *pci)
{
	struct disk *disk = (struct disk *)dev;
	
	filo();
	
	/* past all of the drives */
	dev->index = 0;
	return 0;
}
#define PCI_DEVICE_ID_INTEL_82801CA_11  0x248b

static struct pci_id ide_controllers[] = {
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82801CA_11,    "PIIX4" },
};

static struct pci_driver ide_driver __pci_driver = {
	.type      = DISK_DRIVER,
	.name      = "FILO",
	.probe     = filo_pci_probe,
	.ids       = ide_controllers,
	.id_count  = sizeof(ide_controllers)/sizeof(ide_controllers),
	.class     = PCI_CLASS_STORAGE_IDE,
};
#endif

